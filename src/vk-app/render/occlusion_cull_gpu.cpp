#include "occlusion_cull_gpu.hpp"
#include "./graphics.hpp"
#include "./debug_label.hpp"
#include "frustum_cull_types.hpp"
#include "grove/visual/Camera.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace cull;
using Info = OcclusionCullAgainstDepthPyramidInfo;
using Result = OcclusionCullAgainstDepthPyramidResult;

constexpr uint32_t max_num_datasets = 2;

struct OcclusionCullStats {
  uint32_t num_occluded;
  uint32_t num_visible;
  uint32_t num_frustum_culled;
  uint32_t pad1;
};

struct FrameData {
  gfx::BufferHandle cull_results;
  gfx::BufferHandle cull_stats;
  uint32_t num_active;
  uint32_t num_reserved;
};

struct GPUContext {
  DynamicArray<FrameData, 3> frame_datasets[max_num_datasets];
  gfx::PipelineHandle cull_pipeline;
  gfx::PipelineHandle stats_pipeline;
  OcclusionCullStats latest_cull_stats[max_num_datasets]{};
  Optional<cull::OcclusionCullAgainstDepthPyramidResult> latest_valid_results[max_num_datasets];
  int compute_local_size_x{32};
  bool tried_initialize{};
  bool disabled{};
  bool cull_disabled{};
  bool stats_disabled{};
};

void push_occlusion_defs(glsl::PreprocessorDefinitions& defs) {
  defs.push_back(glsl::make_integer_define(
    "OCCLUSION_CULL_RESULT_OCCLUDED",
    int(OcclusionCullAgainstDepthPyramidResultStatus::Occluded)));
  defs.push_back(glsl::make_integer_define(
    "OCCLUSION_CULL_RESULT_VISIBLE",
    int(OcclusionCullAgainstDepthPyramidResultStatus::Visible)));
}

Optional<gfx::PipelineHandle> create_stats_pipeline(gfx::Context& context, int local_size_x) {
  glsl::LoadComputeProgramSourceParams params{};
  params.file = "cull/occlusion-cull-stats.comp";
  params.compile.defines.push_back(glsl::make_integer_define("LOCAL_SIZE_X", local_size_x));
  push_occlusion_defs(params.compile.defines);
  if (auto src = glsl::make_compute_program_source(params)) {
    return gfx::create_compute_pipeline(&context, std::move(src.value()));
  } else {
    return NullOpt{};
  }
}

Optional<gfx::PipelineHandle> create_cull_pipeline(gfx::Context& context, int local_size_x) {
  glsl::LoadComputeProgramSourceParams params{};
  params.file = "cull/occlusion-cull.comp";
  params.compile.defines.push_back(glsl::make_integer_define("LOCAL_SIZE_X", local_size_x));
  push_occlusion_defs(params.compile.defines);
  if (auto src = glsl::make_compute_program_source(params)) {
    return gfx::create_compute_pipeline(&context, std::move(src.value()));
  } else {
    return NullOpt{};
  }
}

bool reserve_cull_stats(GPUContext& context, FrameData& fd, uint32_t dsi, const Info& info) {
  if (!fd.cull_stats.is_valid()) {
    if (auto buff = gfx::create_storage_buffer(&info.context, sizeof(OcclusionCullStats))) {
      fd.cull_stats = std::move(buff.value());
    } else {
      return false;
    }
  }

  OcclusionCullStats last_stats{};
  fd.cull_stats.read(&last_stats, sizeof(last_stats));
  context.latest_cull_stats[dsi] = last_stats;

  last_stats = {};
  fd.cull_stats.write(&last_stats, sizeof(last_stats));
  return true;
}

bool reserve_cull_results(FrameData& fd, uint32_t dsi, const Info& info) {
  fd.num_active = 0;

  auto& frust_info = info.frustum_cull_infos[dsi].unwrap();

  uint32_t num_res = fd.num_reserved;
  while (num_res < frust_info.num_instances) {
    num_res = num_res == 0 ? 64 : num_res * 2;
  }

  if (num_res != fd.num_reserved) {
    auto buff = gfx::create_buffer(
      &info.context,
      {gfx::BufferUsageFlagBits::Storage},
      {gfx::MemoryTypeFlagBits::DeviceLocal},
      num_res * sizeof(OcclusionCullAgainstDepthPyramidElementResult));
    if (!buff) {
      return false;
    } else {
      fd.cull_results = std::move(buff.value());
      fd.num_reserved = num_res;
    }
  }

  fd.num_active = uint32_t(frust_info.num_instances);
  return true;
}

void try_initialize(GPUContext& context, const Info& info) {
  if (auto pd = create_cull_pipeline(info.context, context.compute_local_size_x)) {
    context.cull_pipeline = std::move(pd.value());
  }
  if (auto pd = create_stats_pipeline(info.context, context.compute_local_size_x)) {
    context.stats_pipeline = std::move(pd.value());
  }
}

void dispatch_stats(GPUContext& context, const Info& info, uint32_t dsi) {
  struct PushConstants {
    Vec4<uint32_t> num_instances_unused;
  };

  auto& pipe = context.stats_pipeline;
  auto& fd = context.frame_datasets[dsi][info.frame_index];
  assert(fd.num_active == info.frustum_cull_infos[dsi].value().num_instances);

  vk::DescriptorSetScaffold scaffold;
  scaffold.set = 0;
  uint32_t bind{};
  vk::push_storage_buffer(
    scaffold, bind++, fd.cull_results.get(),
    fd.num_active * sizeof(OcclusionCullAgainstDepthPyramidElementResult));
  vk::push_storage_buffer(
    scaffold, bind++, *info.frustum_cull_infos[dsi].value().cull_results,
    fd.num_active * sizeof(FrustumCullResult));
  vk::push_storage_buffer(
    scaffold, bind++, fd.cull_stats.get(), sizeof(OcclusionCullStats));

  auto desc_set = gfx::require_updated_descriptor_set(&info.context, scaffold, pipe);
  if (!desc_set) {
    return;
  }

  vk::cmd::bind_compute_pipeline(info.cmd, pipe.get());
  vk::cmd::bind_compute_descriptor_sets(info.cmd, pipe.get_layout(), 0, 1, &desc_set.value());

  PushConstants pcs{};
  pcs.num_instances_unused = Vec4<uint32_t>{uint32_t(fd.num_active), 0, 0, 0};
  vk::cmd::push_constants(info.cmd, pipe.get_layout(), VK_SHADER_STAGE_COMPUTE_BIT, &pcs);
  auto nx = int(std::ceil(double(fd.num_active) / double(context.compute_local_size_x)));
  vkCmdDispatch(info.cmd, nx, 1, 1);
}

bool dispatch_cull(GPUContext& context, const Info& info, uint32_t dsi) {
  auto db_label = GROVE_VK_SCOPED_DEBUG_LABEL(info.cmd, "occlusion_cull_against_depth_pyramid");
  (void) db_label;

  struct PushConstants {
    Mat4f proj_view;
    Vec4<uint32_t> num_instances_max_mip_unused;
    Vec4f disabled_root_dimensions;
  };

  auto& pipe = context.cull_pipeline;
  auto& frust_info = info.frustum_cull_infos[dsi].value();
  auto& pyr_info = info.depth_pyramid_info.value();
  auto& fd = context.frame_datasets[dsi][info.frame_index];
  assert(frust_info.num_instances == fd.num_active);

  auto sampler = gfx::get_image_sampler_nearest_edge_clamp(&info.context);

  vk::DescriptorSetScaffold scaffold;
  scaffold.set = 0;
  uint32_t bind{};
  vk::push_storage_buffer(
    scaffold, bind++, *frust_info.instances,
    frust_info.num_instances * sizeof(FrustumCullInstance));
  vk::push_storage_buffer(
    scaffold, bind++, *frust_info.cull_results,
    frust_info.num_instances * sizeof(FrustumCullResult));
  vk::push_storage_buffer(
    scaffold, bind++, fd.cull_results.get(),
    frust_info.num_instances * sizeof(OcclusionCullAgainstDepthPyramidElementResult));
  vk::push_combined_image_sampler(scaffold, bind++, pyr_info.depth_pyramid_image, sampler);

  auto desc_set = gfx::require_updated_descriptor_set(&info.context, scaffold, pipe);
  if (!desc_set) {
    return false;
  }

  vk::cmd::bind_compute_pipeline(info.cmd, pipe.get());
  vk::cmd::bind_compute_descriptor_sets(info.cmd, pipe.get_layout(), 0, 1, &desc_set.value());

  auto proj = info.camera.get_projection();
  proj[1] = -proj[1];
  auto proj_view = proj * info.camera.get_view();

  PushConstants pcs{};
  pcs.proj_view = proj_view;
  pcs.num_instances_max_mip_unused = Vec4<uint32_t>{
    uint32_t(frust_info.num_instances), pyr_info.depth_pyramid_image_max_mip, 0, 0};
  pcs.disabled_root_dimensions = Vec4f{
    float(context.cull_disabled),
    float(pyr_info.depth_pyramid_image_extent.width),
    float(pyr_info.depth_pyramid_image_extent.height),
    0
  };

  vk::cmd::push_constants(info.cmd, pipe.get_layout(), VK_SHADER_STAGE_COMPUTE_BIT, &pcs);

  auto nx = int(std::ceil(double(frust_info.num_instances) / double(context.compute_local_size_x)));
  vkCmdDispatch(info.cmd, nx, 1, 1);

  return true;
}

void insert_post_cull_pipeline_barrier(VkCommandBuffer cmd) {
  vk::PipelineBarrierDescriptor barrier_desc{};
  barrier_desc.stages.src = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
  barrier_desc.stages.dst = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

  VkMemoryBarrier memory_barrier{};
  memory_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
  memory_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier_desc.num_memory_barriers = 1;
  barrier_desc.memory_barriers = &memory_barrier;

  vk::cmd::pipeline_barrier(cmd, &barrier_desc);
}

void main_cull(GPUContext& context, const Info& info) {
  assert(info.num_cull_infos <= max_num_datasets);

  for (auto& res : context.latest_valid_results) {
    res = NullOpt{};
  }

  if (context.disabled) {
    return;
  }

  if (!context.tried_initialize) {
    try_initialize(context, info);
    context.tried_initialize = true;
  }

  if (!context.cull_pipeline.is_valid() || !context.stats_pipeline.is_valid()) {
    return;
  }

  for (uint32_t i = 0; i < info.num_cull_infos; i++) {
    while (info.frame_index >= uint32_t(context.frame_datasets[i].size())) {
      context.frame_datasets[i].emplace_back();
    }
  }

  OcclusionCullAgainstDepthPyramidResults valid_results{};

  bool all_success{true};
  bool any_success{};
  for (uint32_t i = 0; i < info.num_cull_infos; i++) {
    if (!info.frustum_cull_infos[i] || !info.depth_pyramid_info) {
      all_success = false;
      continue;
    }

    auto& fd = context.frame_datasets[i][info.frame_index];
    if (!reserve_cull_results(fd, i, info)) {
      all_success = false;
      continue;
    }

    if (!reserve_cull_stats(context, fd, i, info)) {
      all_success = false;
      continue;
    }

    if (!dispatch_cull(context, info, i)) {
      all_success = false;
    } else {
      any_success = true;

      Result ok_result{};
      ok_result.num_elements = fd.num_active;
      ok_result.result_buffer = fd.cull_results.get();
      context.latest_valid_results[i] = ok_result;
      valid_results.results[i] = ok_result;
    }
  }

  if (any_success) {
    insert_post_cull_pipeline_barrier(info.cmd);
  }

  if (all_success) {
    if (!context.stats_disabled) {
      for (uint32_t i = 0; i < info.num_cull_infos; i++) {
        dispatch_stats(context, info, i);
      }
    }
  }
}

void begin_frame(GPUContext& context, const bool* frustum_cull_data_modified, uint32_t num_datasets) {
  assert(num_datasets <= max_num_datasets);
  for (uint32_t i = 0; i < num_datasets; i++) {
    if (frustum_cull_data_modified[i]) {
      context.latest_valid_results[i] = NullOpt{};
    }
  }
}

struct {
  GPUContext context;
} globals;

} //  anon

void cull::occlusion_cull_against_depth_pyramid(const Info& info) {
  main_cull(globals.context, info);
}

Optional<OcclusionCullAgainstDepthPyramidResult>
cull::get_previous_occlusion_cull_against_depth_pyramid_result(uint32_t input_index) {
  assert(input_index < max_num_datasets);
  return globals.context.latest_valid_results[input_index];
}

void cull::occlusion_cull_against_depth_pyramid_begin_frame(
  const bool* frustum_cull_data_modified, uint32_t num_datasets) {
  //
  begin_frame(globals.context, frustum_cull_data_modified, num_datasets);
}

OcclusionCullAgainstDepthPyramidStats
cull::get_occlusion_cull_against_depth_pyramid_stats(uint32_t input_index) {
  assert(input_index < max_num_datasets);
  OcclusionCullAgainstDepthPyramidStats stats{};
  auto& src = globals.context.latest_cull_stats[input_index];
  stats.prev_num_occluded = src.num_occluded;
  stats.prev_num_visible = src.num_visible;
  stats.prev_num_total = src.num_visible + src.num_occluded;
  stats.prev_num_frustum_culled = src.num_frustum_culled;
  stats.prev_num_purely_occlusion_culled = src.num_occluded - src.num_frustum_culled;
  return stats;
}

void cull::terminate_occlusion_cull_against_depth_pyramid() {
  globals.context = {};
}

void cull::push_read_occlusion_cull_preprocessor_defines(glsl::PreprocessorDefinitions& defines) {
  defines.push_back(
    glsl::make_integer_define(
      "OCCLUSION_CULL_RESULT_OCCLUDED",
      int(cull::OcclusionCullAgainstDepthPyramidResultStatus::Occluded)));
  defines.push_back(
    glsl::make_integer_define(
      "OCCLUSION_CULL_RESULT_VISIBLE",
      int(cull::OcclusionCullAgainstDepthPyramidResultStatus::Visible)));
}

GROVE_NAMESPACE_END
