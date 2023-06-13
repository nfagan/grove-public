#include "render_tree_leaves_gpu.hpp"
#include "render_tree_leaves_types.hpp"
#include "shadow.hpp"
#include "csm.hpp"
#include "../util/texture_io.hpp"
#include "../vk/vk.hpp"
#include "SampledImageManager.hpp"
#include "DynamicSampledImageManager.hpp"
#include "./debug_label.hpp"
#include "./graphics.hpp"
#include "frustum_cull_types.hpp"
#include "foliage_occlusion_types.hpp"
#include "occlusion_cull_gpu.hpp"
#include "../procedural_flower/geometry.hpp"
#include "grove/env.hpp"
#include "grove/visual/Camera.hpp"
#include "grove/visual/Image.hpp"
#include "grove/visual/geometry.hpp"
#include "grove/load/image.hpp"
#include "grove/common/common.hpp"

/*
 * @NOTE (1/11/23) -- Weird issue on macos where, for some frame captures with Xcode, the instance
 * count fields of indirect draw commands are some multiple (e.g. 5) of the true number of
 * instances. the time to execute the draw is consistent with there being this scaling factor, at
 * least according to Xcode. The issue goes away if we issue a cmdCopyBuffer to clear the instance
 * counts to 0.
 */

GROVE_NAMESPACE_BEGIN

namespace {

using namespace foliage;
using BeginFrameInfo = TreeLeavesRendererBeginFrameInfo;
using EarlyComputeInfo = TreeLeavesRendererEarlyGraphicsComputeInfo;
using PostForwardComputeInfo = TreeLeavesRendererPostForwardGraphicsComputeInfo;
using RenderForwardInfo = TreeLeavesRenderForwardInfo;
using IndirectDrawCommand = VkDrawIndexedIndirectCommand;

struct Config {
  static constexpr uint32_t high_lod_index = 1;
  static constexpr uint32_t low_lod_index = 2;
  static constexpr uint32_t initial_num_instances_reserve = 128;
//  static constexpr uint32_t initial_num_instances_reserve = 512000;
};

struct DrawInstanceIndex {
  uint32_t index;
};

struct GenLODIndicesPushConstantData {
  Vec4<uint32_t> num_instances_unused;
  Vec4f camera_position;
};

struct GatherNewlyDisoccludedIndicesPushConstantData {
  Vec4<uint32_t> num_instances_unused;
};

struct PartitionLODIndicesPushConstantData {
  Vec4<uint32_t> num_instances_target_lod_index_unused;
};

struct RenderForwardsPushConstantData {
  Mat4f projection_view;
  Vec4f data0;
};

struct RenderForwardsUniformData {
  csm::SunCSMSampleData csm_sample_data;
  Mat4f view;
  Mat4f shadow_proj_view;
  Vec4f camera_position_alpha_test_enabled;
  Vec4f wind_world_bound_xz;
  Vec4f wind_displacement_limits_wind_strength_limits;
  Vec4f sun_position;
  Vec4f sun_color;
};

RenderForwardsUniformData make_render_forwards_uniform_data(const Camera& camera,
                                                            const csm::CSMDescriptor& csm_desc,
                                                            const Vec4f& wind_world_bound_xz,
                                                            const Vec2f& wind_displace_limits,
                                                            const Vec2f& wind_strength_limits,
                                                            const Vec3f& sun_position,
                                                            const Vec3f& sun_color) {
  RenderForwardsUniformData result;
  result.csm_sample_data = csm::make_sun_csm_sample_data(csm_desc);
  result.view = camera.get_view();
  result.shadow_proj_view = csm_desc.light_shadow_sample_view;
  result.camera_position_alpha_test_enabled = Vec4f{camera.get_position(), 1.0f};
  result.wind_world_bound_xz = wind_world_bound_xz;
  result.wind_displacement_limits_wind_strength_limits = Vec4f{
    wind_displace_limits.x, wind_displace_limits.y,
    wind_strength_limits.x, wind_strength_limits.y
  };
  result.sun_position = Vec4f{sun_position, 0.0f};
  result.sun_color = Vec4f{sun_color, 0.0f};
  return result;
}

GenLODIndicesPushConstantData make_gen_lod_indices_push_constant_data(uint32_t num_instances,
                                                                      const Camera& camera) {
  GenLODIndicesPushConstantData result;
  result.num_instances_unused = Vec4<uint32_t>{num_instances, 0, 0, 0};
  result.camera_position = Vec4f{camera.get_position(), 0.0f};
  return result;
}

PartitionLODIndicesPushConstantData
make_partition_lod_indices_push_constant_data(uint32_t num_instances, uint32_t target_lod_index) {
  PartitionLODIndicesPushConstantData result;
  result.num_instances_target_lod_index_unused = Vec4<uint32_t>{num_instances, target_lod_index, 0, 0};
  return result;
}

RenderForwardsPushConstantData
make_render_forwards_push_constant_data(const Camera& camera, float elapsed_time) {
  auto proj = camera.get_projection();
  proj[1] = -proj[1];
  RenderForwardsPushConstantData result;
  result.projection_view = proj * camera.get_view();
  result.data0 = Vec4f{elapsed_time, 0.0f, 0.0f, 0.0f};
  return result;
}

struct GPUContext {
  struct DrawIndexedBuffers {
    vk::BufferSystem::BufferHandle indirect_draw_params;
    vk::BufferSystem::BufferHandle indices;
  };

  struct GeometryBuffer {
    gfx::BufferHandle geometry;
    gfx::BufferHandle indices;
    uint32_t num_vertex_indices{};
  };

  struct GeometryBuffers {
    GeometryBuffer lod0;
    GeometryBuffer lod1;
  };

  struct ModifiedInstances {
    void clear() {
      modified = false;
      ranges_invalidated = false;
      modified_ranges.clear();
    }

    bool modified{};
    bool ranges_invalidated{};
    DistinctRanges<uint32_t> modified_ranges;
  };

  struct FrameData {
    ModifiedInstances modified_instances;

    uint32_t num_instances_reserved{};
    uint32_t num_instances{};

    uint32_t num_instance_groups_reserved{};
    uint32_t num_instance_groups{};

    uint32_t num_shadow_instances{};

    uint32_t num_cpu_occlusion_clusters_reserved{};
    uint32_t num_cpu_occlusion_clusters{};
    uint32_t num_cpu_occlusion_cluster_group_offsets_reserved{};
    uint32_t num_cpu_occlusion_cluster_group_offsets{};

    vk::BufferSystem::BufferHandle instances;
    vk::BufferSystem::BufferHandle instance_component_indices;
    vk::BufferSystem::BufferHandle lod_compute_instances;
    vk::BufferSystem::BufferHandle computed_lod_indices;
    vk::BufferSystem::BufferHandle computed_lod_dependent_data;
    vk::BufferSystem::BufferHandle shadow_render_indices;

    vk::BufferSystem::BufferHandle instance_groups;

    vk::BufferSystem::BufferHandle cpu_occlusion_clusters;
    vk::BufferSystem::BufferHandle cpu_occlusion_cluster_group_offsets;

    vk::BufferSystem::BufferHandle uniform_buffer;

    DrawIndexedBuffers lod0_indices;
    DrawIndexedBuffers lod1_indices;
    DrawIndexedBuffers post_forward_lod0_indices;
    DrawIndexedBuffers post_forward_lod1_indices;
  };

  DynamicArray<FrameData, 3> frame_data;
  std::bitset<32> cpu_occlusion_frame_data_modified{};
  std::bitset<32> instance_groups_modified{};
  bool cpu_occlusion_data_modified{};

  Optional<int> set_compute_local_size_x;
  int compute_local_size_x{32};

  vk::BufferSystem::BufferHandle transfer_draw_command_buff0;
  vk::BufferSystem::BufferHandle transfer_draw_command_buff1;

  gfx::PipelineHandle gen_lod_indices_pipeline;
  gfx::PipelineHandle gen_lod_indices_cpu_occlusion_pipeline;
  gfx::PipelineHandle gen_lod_indices_gpu_occlusion_no_cpu_occlusion_pipeline;
  gfx::PipelineHandle gen_lod_indices_gpu_occlusion_no_cpu_occlusion_high_lod_disabled_pipeline;
  gfx::PipelineHandle partition_lod_indices_pipeline;
  gfx::PipelineHandle gather_newly_disoccluded_indices_pipeline;

  gfx::PipelineHandle render_forwards_array_images_pipeline;
  gfx::PipelineHandle render_forwards_array_images_alpha_to_coverage_pipeline;
  gfx::PipelineHandle render_forwards_mix_color_array_images_pipeline;
  gfx::PipelineHandle render_forwards_mix_color_single_channel_alpha_images_pipeline;
  gfx::PipelineHandle render_forwards_mix_color_array_images_alpha_to_coverage_pipeline;
  gfx::PipelineHandle render_shadow_pipeline;

  gfx::PipelineHandle render_post_process_mix_color_array_images_pipeline;

  Optional<VkDescriptorSet> gen_lod_desc_set0;
  Optional<VkDescriptorSet> gen_lod_cpu_occlusion_desc_set0;
  Optional<VkDescriptorSet> gen_lod_gpu_occlusion_no_cpu_occlusion_desc_set0;
  Optional<VkDescriptorSet> partition_lod0_desc_set0;
  Optional<VkDescriptorSet> partition_lod1_desc_set0;
  Optional<VkDescriptorSet> render_forwards_array_images_desc_set0;
  Optional<VkDescriptorSet> render_shadow_desc_set0;

  GenLODIndicesPushConstantData gen_lod_indices_pc_data{};
  PartitionLODIndicesPushConstantData partition_lod_indices_pc_data0{};
  PartitionLODIndicesPushConstantData partition_lod_indices_pc_data1{};
  RenderForwardsPushConstantData render_forwards_pc_data{};

  Optional<vk::DynamicSampledImageManager::Handle> wind_displacement_image;
  Optional<vk::SampledImageManager::Handle> alpha_array_image;
  Optional<vk::SampledImageManager::Handle> hemisphere_color_array_image;
  Optional<vk::SampledImageManager::Handle> alpha_array_image_tiny;
  Optional<vk::SampledImageManager::Handle> single_channel_alpha_array_image_tiny;
  Optional<vk::SampledImageManager::Handle> hemisphere_color_array_image_tiny;
  Optional<vk::SampledImageManager::Handle> mip_mapped_alpha_array_image_tiny;
  Optional<vk::SampledImageManager::Handle> mip_mapped_hemisphere_color_array_image_tiny;

  Optional<GeometryBuffers> geometry_buffers;

  std::vector<uint32_t> cpu_shadow_render_indices;

  uint32_t max_instance_alpha_image_index{};
  uint32_t max_instance_color_image_index{};

  IndirectDrawCommand prev_written_lod0_indirect_command{};
  IndirectDrawCommand prev_written_lod1_indirect_command{};
  IndirectDrawCommand prev_written_post_forward_lod0_indirect_command{};
  IndirectDrawCommand prev_written_post_forward_lod1_indirect_command{};
  uint32_t num_shadow_instances_drawn{};

  bool buffers_valid{true};
  bool compute_pipelines_valid{};
  bool try_initialize{true};
  bool need_recreate_pipelines{};
  bool disable_pcf{};
  bool disable_color_mix{};
  bool disable_high_lod{};
  bool do_clear_indirect_commands_via_explicit_buffer_copy{};

  bool began_frame{};
  bool did_generate_lod_indices_with_gpu_occlusion{};
  bool did_generate_post_forward_draw_indices{};
  bool disabled{};
  bool forward_rendering_disabled{};
  bool shadow_rendering_disabled{};
  bool render_forward_with_alpha_to_coverage{};
  bool render_forward_with_color_image_mix{true};
  bool generate_lod_indices_with_cpu_occlusion{};
  bool prefer_tiny_array_images{true};
  bool prefer_single_channel_alpha_images{};
  bool prefer_mip_mapped_images{};
  bool prefer_gpu_occlusion{true};
  bool post_forward_compute_disabled{};
  bool gui_feedback_did_render_with_gpu_occlusion{};
  uint32_t max_shadow_cascade_index{1};
  TreeLeavesRenderParams render_params{};
};

Optional<std::vector<Image<uint8_t>>> load_images(const std::string& im_dir, const char** im_names,
                                                  int num_images, int expect_components) {
  std::vector<Image<uint8_t>> images;
  for (int i = 0; i < num_images; i++) {
    auto im_p = im_dir + im_names[i];
    bool success{};
    auto im = load_image(im_p.c_str(), &success, true);
    if (!success || im.num_components_per_pixel != expect_components) {
      return NullOpt{};
    }
    images.emplace_back(std::move(im));
  }
  return Optional<std::vector<Image<uint8_t>>>(std::move(images));
}

Optional<vk::SampledImageManager::Handle>
create_mip_mapped_alpha_test_array_image(const BeginFrameInfo& info) {
  auto im_dir = std::string{GROVE_ASSET_DIR} + "/textures/tree-leaves-tiny-mip/";

  const char* im_names[5] = {
    "maple-leaf-revisit.png",
    "oak-leaf.png",
    "elm-leaf.png",
    "broad-leaf1-no-border.png",
    "thin-leaves1.png"
  };

  constexpr int num_levels = 6;
  std::unique_ptr<uint8_t[]> levels[num_levels];
  const uint8_t* level_ptrs[num_levels];

  int rw{};
  int rh{};
  for (int i = 0; i < num_levels; i++) {
    auto mip_dir = im_dir + std::to_string(i) + "/";
    auto images = load_images(mip_dir, im_names, 5, 4);

    if (!images) {
      return NullOpt{};
    }

    auto res = pack_texture_layers(images.value());
    if (!res) {
      return NullOpt{};
    }

    if (i == 0) {
      rw = images.value()[0].width;
      rh = images.value()[0].height;
    }

    level_ptrs[i] = res.get();
    levels[i] = std::move(res);
  }

  vk::SampledImageManager::ImageCreateInfo create_info{};
  create_info.descriptor = {
    image::Shape::make_3d(rw, rh, 5),
    image::Channels::make_uint8n(4)
  };
  create_info.mip_levels = (const void**) level_ptrs;
  create_info.num_mip_levels = num_levels;
  create_info.int_conversion = IntConversion::UNorm;
  create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
  create_info.image_type = vk::SampledImageManager::ImageType::Image2DArray;
  create_info.sample_in_stages = {vk::PipelineStage::FragmentShader};
  return info.sampled_image_manager.create_sync(create_info);
}

Optional<vk::SampledImageManager::Handle>
create_mip_mapped_color_array_image(const BeginFrameInfo& info) {
  auto im_dir = std::string{GROVE_ASSET_DIR} + "/textures/experiment-tiny-mip/";

  const char* im_names[5] = {
    "tiled1-small.png",
    "tiled2-small.png",
    "japanese-maple.png",
    "fall_yellow.png",
    "fall_orange.png",
  };

  constexpr int num_levels = 6;
  std::unique_ptr<uint8_t[]> levels[num_levels];
  const uint8_t* level_ptrs[num_levels];

  int rw{};
  int rh{};
  for (int i = 0; i < num_levels; i++) {
    auto mip_dir = im_dir + std::to_string(i) + "/";
    auto images = load_images(mip_dir, im_names, 5, 4);

    if (!images) {
      return NullOpt{};
    }

    auto res = pack_texture_layers(images.value());
    if (!res) {
      return NullOpt{};
    }

    if (i == 0) {
      rw = images.value()[0].width;
      rh = images.value()[0].height;
    }

    level_ptrs[i] = res.get();
    levels[i] = std::move(res);
  }

  vk::SampledImageManager::ImageCreateInfo create_info{};
  create_info.descriptor = {
    image::Shape::make_3d(rw, rh, 5),
    image::Channels::make_uint8n(4)
  };
  create_info.mip_levels = (const void**) level_ptrs;
  create_info.num_mip_levels = uint32_t(num_levels);
  create_info.format = VK_FORMAT_R8G8B8A8_SRGB;
  create_info.image_type = vk::SampledImageManager::ImageType::Image2DArray;
  create_info.sample_in_stages = {vk::PipelineStage::FragmentShader};
  return info.sampled_image_manager.create_sync(create_info);
}

Optional<vk::SampledImageManager::Handle>
create_alpha_test_array_image(const BeginFrameInfo& info, bool tiny, bool one_channel = false) {
  auto im_dir = std::string{GROVE_ASSET_DIR} + "/textures/";
  im_dir += tiny ? "tree-leaves-tiny/" : "tree-leaves/";

  const char* im_names[5] = {
    "maple-leaf-revisit.png",
    "oak-leaf.png",
    "elm-leaf.png",
    "broad-leaf1-no-border.png",
    "thin-leaves1.png"
  };

  auto images = load_images(im_dir, im_names, 5, 4);
  if (!images) {
    return NullOpt{};
  }

  if (one_channel) {
    for (auto& im : images.value()) {
      auto new_data = std::make_unique<uint8_t[]>(im.width * im.height);
      for (int i = 0; i < im.width * im.height; i++) {
        new_data[i] = im.data[i * 4 + 3];
      }
      im.data = std::move(new_data);
      im.num_components_per_pixel = 1;
    }
  }

  auto res = pack_texture_layers(images.value());
  if (!res) {
    return NullOpt{};
  }

  auto& ims = images.value();
  vk::SampledImageManager::ImageCreateInfo create_info{};
  create_info.descriptor = {
    image::Shape::make_3d(ims[0].width, ims[0].height, int(ims.size())),
    image::Channels::make_uint8n(one_channel ? 1 : 4)
  };
  create_info.data = res.get();
  create_info.int_conversion = IntConversion::UNorm;
  create_info.format = one_channel ? VK_FORMAT_R8_UNORM : VK_FORMAT_R8G8B8A8_UNORM;
  create_info.image_type = vk::SampledImageManager::ImageType::Image2DArray;
  create_info.sample_in_stages = {vk::PipelineStage::FragmentShader};
  return info.sampled_image_manager.create_sync(create_info);
}

Optional<vk::SampledImageManager::Handle>
create_color_array_image(const BeginFrameInfo& info, bool tiny) {
  auto im_dir = std::string{GROVE_ASSET_DIR} + "/textures/";
  im_dir += tiny ? "experiment-tiny/" : "experiment/";

#if 1
  const char* im_names[5] = {
    "tiled1-small.png",
    "tiled2-small.png",
    "japanese-maple.png",
    "fall_yellow.png",
    "fall_orange.png",
  };
#else
  const char* im_names[5] = {
    "tiled1-small.png",
    "tiled2-small.png",
    "tiled3-small.png",
    "tiled4-small.png",
    "tiled5-small.png",
  };
#endif

  auto images = load_images(im_dir, im_names, 5, 4);
  if (!images) {
    return NullOpt{};
  }
  auto res = pack_texture_layers(images.value());
  if (!res) {
    return NullOpt{};
  }

  auto& ims = images.value();
  vk::SampledImageManager::ImageCreateInfo create_info{};
  create_info.descriptor = {
    image::Shape::make_3d(ims[0].width, ims[0].height, int(ims.size())),
    image::Channels::make_uint8n(4)
  };
  create_info.data = res.get();
  create_info.format = VK_FORMAT_R8G8B8A8_SRGB;
  create_info.image_type = vk::SampledImageManager::ImageType::Image2DArray;
  create_info.sample_in_stages = {vk::PipelineStage::FragmentShader};
  return info.sampled_image_manager.create_sync(create_info);
}

Optional<glsl::VertFragProgramSource>
create_render_forward_program_source(
  bool use_alpha_to_coverage, bool enable_color_image_mix,
  bool disable_pcf, bool disable_color_mix, bool single_alpha_channel) {
  //
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "proc-tree/gpu-driven-leaves.vert";
  params.frag_file = "proc-tree/gpu-driven-leaves.frag";
  auto defs = csm::make_default_sample_shadow_preprocessor_definitions();
  params.compile.vert_defines.insert(params.compile.vert_defines.end(), defs.begin(), defs.end());
  params.compile.frag_defines.insert(params.compile.frag_defines.end(), defs.begin(), defs.end());
  params.compile.vert_defines.push_back(glsl::make_define("USE_ARRAY_IMAGES"));
  params.compile.frag_defines.push_back(glsl::make_define("USE_ARRAY_IMAGES"));
  if (use_alpha_to_coverage) {
    params.compile.frag_defines.push_back(glsl::make_define("USE_ALPHA_TO_COVERAGE"));
  }
  if (enable_color_image_mix) {
    params.compile.frag_defines.push_back(glsl::make_define("ENABLE_COLOR_IMAGE_MIX"));
    params.compile.vert_defines.push_back(glsl::make_define("ENABLE_COLOR_IMAGE_MIX"));
  }
  if (disable_pcf) {
    params.compile.frag_defines.push_back(glsl::make_define("NO_PCF"));
  }
  if (disable_color_mix) {
    params.compile.frag_defines.push_back(glsl::make_define("NO_COLOR_MIX"));
  }
  if (single_alpha_channel) {
    params.compile.frag_defines.push_back(glsl::make_define("USE_SINGLE_CHANNEL_ALPHA_IMAGE"));
  }
  return glsl::make_vert_frag_program_source(params);
}

Optional<glsl::VertFragProgramSource> create_render_shadow_program_source() {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "proc-tree/gpu-driven-leaves.vert";
  params.frag_file = "shadow/empty.frag";
  params.compile.vert_defines.push_back(glsl::make_define("IS_SHADOW"));
  return glsl::make_vert_frag_program_source(params);
}

std::array<VertexBufferDescriptor, 2> make_render_vertex_buffer_descs() {
  std::array<VertexBufferDescriptor, 2> vb_descs;
  vb_descs[0].add_attribute(AttributeDescriptor::float2(0));
  vb_descs[1].add_attribute(AttributeDescriptor::unconverted_unsigned_intn(1, 1, 1));
  return vb_descs;
}

Optional<gfx::PipelineHandle> create_render_pipeline(
  gfx::Context* context, bool enable_alpha_to_cov, bool enable_color_image_mix,
  bool disable_pcf, bool disable_color_mix, bool single_alpha_channel,
  const gfx::RenderPassHandle& pass) {
  //
  auto src = create_render_forward_program_source(
    enable_alpha_to_cov, enable_color_image_mix, disable_pcf,
    disable_color_mix, single_alpha_channel);
  if (!src) {
    return NullOpt{};
  }

  auto vb_descs = make_render_vertex_buffer_descs();
  gfx::GraphicsPipelineCreateInfo create_info{};
  create_info.vertex_buffer_descriptors = vb_descs.data();
  create_info.num_vertex_buffer_descriptors = uint32_t(vb_descs.size());
  create_info.num_color_attachments = 1;
  create_info.disable_cull_face = true;
  create_info.enable_alpha_to_coverage = enable_alpha_to_cov;
  return gfx::create_pipeline(context, std::move(src.value()), create_info, pass);
}

Optional<gfx::PipelineHandle> create_render_forward_pipeline(
  gfx::Context* context, bool enable_alpha_to_cov, bool enable_color_image_mix,
  bool disable_pcf, bool disable_color_mix, bool single_alpha_channel = false) {
  //
  if (auto pass = gfx::get_forward_write_back_render_pass_handle(context)) {
    return create_render_pipeline(
      context, enable_alpha_to_cov, enable_color_image_mix,
      disable_pcf, disable_color_mix, single_alpha_channel, pass.value());
  } else {
    return NullOpt{};
  }
}

Optional<gfx::PipelineHandle> create_render_post_forward_pipeline(
  gfx::Context* context, bool enable_alpha_to_cov, bool enable_color_image_mix,
  bool disable_pcf, bool disable_color_mix, bool single_alpha_channel = false) {
  //
  if (auto pass = gfx::get_post_forward_render_pass_handle(context)) {
    return create_render_pipeline(
      context, enable_alpha_to_cov, enable_color_image_mix,
      disable_pcf, disable_color_mix, single_alpha_channel, pass.value());
  } else {
    return NullOpt{};
  }
}

Optional<gfx::PipelineHandle> create_render_shadow_pipeline(gfx::Context* context) {
  auto src = create_render_shadow_program_source();
  if (!src) {
    return NullOpt{};
  }

  auto pass = gfx::get_shadow_render_pass_handle(context);
  if (!pass) {
    return NullOpt{};
  }

  auto vb_descs = make_render_vertex_buffer_descs();
  gfx::GraphicsPipelineCreateInfo create_info{};
  create_info.vertex_buffer_descriptors = vb_descs.data();
  create_info.num_vertex_buffer_descriptors = uint32_t(vb_descs.size());
  create_info.num_color_attachments = 0;
  create_info.disable_cull_face = true;
  return gfx::create_pipeline(context, std::move(src.value()), create_info, pass.value());
}

Optional<gfx::PipelineHandle>
create_gather_newly_disoccluded_indices_pipeline(gfx::Context* context, int local_size_x) {
  glsl::LoadComputeProgramSourceParams params{};
  params.file = "foliage-cull/gather-newly-disoccluded-indices.comp";
  params.compile.defines.push_back(glsl::make_integer_define("LOCAL_SIZE_X", local_size_x));
  cull::push_read_occlusion_cull_preprocessor_defines(params.compile.defines);

  auto src = glsl::make_compute_program_source(params);
  if (!src) {
    return NullOpt{};
  }

  return gfx::create_compute_pipeline(context, std::move(src.value()));
}

Optional<gfx::PipelineHandle> create_gen_lod_indices_pipeline(
  gfx::Context* context, int local_size_x, bool use_cpu_occlusion, bool use_gpu_occlusion,
  bool disable_high_lod = false) {
  //
  glsl::LoadComputeProgramSourceParams params{};
  params.file = "foliage-cull/gen-lod-indices.comp";

  params.compile.defines.push_back(glsl::make_integer_define("LOCAL_SIZE_X", local_size_x));

  if (use_cpu_occlusion) {
    params.compile.defines.push_back(glsl::make_define("USE_CPU_OCCLUSION"));
    auto inst_def = glsl::make_integer_define(
      "MAX_NUM_INSTANCES_PER_CLUSTER",
      foliage_occlusion::Config::max_num_instances_per_cluster);
    params.compile.defines.push_back(inst_def);
  }

  if (use_gpu_occlusion) {
    params.compile.defines.push_back(glsl::make_define("USE_GPU_OCCLUSION"));
    cull::push_read_occlusion_cull_preprocessor_defines(params.compile.defines);
  }

  if (disable_high_lod) {
    params.compile.defines.push_back(glsl::make_define("DISABLE_HIGH_LOD"));
  }

  auto src = glsl::make_compute_program_source(params);
  if (!src) {
    return NullOpt{};
  }

  return gfx::create_compute_pipeline(context, std::move(src.value()));
}

Optional<gfx::PipelineHandle>
create_partition_lod_indices_pipeline(gfx::Context* context, int local_size_x) {
  glsl::LoadComputeProgramSourceParams params{};
  params.file = "foliage-cull/partition-lod-indices.comp";

  params.compile.defines.push_back(glsl::make_integer_define("LOCAL_SIZE_X", local_size_x));
  auto src = glsl::make_compute_program_source(params);
  if (!src) {
    return NullOpt{};
  }

  return gfx::create_compute_pipeline(context, std::move(src.value()));
}

Optional<GPUContext::GeometryBuffer> create_geometry_buffer(
  const std::vector<float>& pad_geom, const std::vector<uint16_t>& indices, const BeginFrameInfo& info) {
  //
  auto geom = gfx::create_device_local_vertex_buffer_sync(
    info.context, pad_geom.size() * sizeof(float), pad_geom.data());
  if (!geom) {
    return NullOpt{};
  }

  auto inds = gfx::create_device_local_index_buffer_sync(
    info.context, indices.size() * sizeof(uint16_t), indices.data());
  if (!inds) {
    return NullOpt{};
  }

  GPUContext::GeometryBuffer result{};
  result.geometry = std::move(geom.value());
  result.indices = std::move(inds.value());
  result.num_vertex_indices = uint32_t(indices.size());
  return Optional<GPUContext::GeometryBuffer>(std::move(result));
}

Optional<GPUContext::GeometryBuffers> create_geometry_buffers(const BeginFrameInfo& info) {
  auto norm_geom = [](std::vector<float>& pad_geom, const GridGeometryParams& grid_geom) -> void {
    for (uint32_t i = 0; i < uint32_t(pad_geom.size()/2); i++) {
      float& x = pad_geom[i * 2];
      float& y = pad_geom[i * 2 + 1];
      x /= float(grid_geom.num_pts_x / 2);
      y = (y / float(grid_geom.num_pts_z - 1)) * 2.0f - 1.0f;
      assert(x >= -1.0f && x <= 1.0f);
      assert(y >= -1.0f && y <= 1.0f);
    }
  };

  auto lod0 = [&]() {
    const GridGeometryParams grid_geom{5, 2};
    auto geom = make_reflected_grid_indices(grid_geom);
    auto tris = triangulate_reflected_grid(grid_geom);
    norm_geom(geom, grid_geom);
    return create_geometry_buffer(geom, tris, info);
  }();

  if (!lod0) {
    return NullOpt{};
  }

  auto lod1 = [&]() {
    auto geom = geometry::quad_positions(false);
    auto inds = geometry::quad_indices();
    return create_geometry_buffer(geom, inds, info);
  }();

  if (!lod1) {
    return NullOpt{};
  }

  GPUContext::GeometryBuffers result{};
  result.lod0 = std::move(lod0.value());
  result.lod1 = std::move(lod1.value());
  return Optional<GPUContext::GeometryBuffers>(std::move(result));
}

void init_geometry(GPUContext& context, const BeginFrameInfo& info) {
  if (auto buffs = create_geometry_buffers(info)) {
    context.geometry_buffers = std::move(buffs.value());
  }
}

void init_pipelines(GPUContext& context, const BeginFrameInfo& info) {
  context.compute_pipelines_valid = false;

  if (auto pd = create_gen_lod_indices_pipeline(info.context, context.compute_local_size_x, false, false)) {
    context.gen_lod_indices_pipeline = std::move(pd.value());
  } else {
    return;
  }

  if (auto pd = create_gen_lod_indices_pipeline(info.context, context.compute_local_size_x, true, false)) {
    context.gen_lod_indices_cpu_occlusion_pipeline = std::move(pd.value());
  } else {
    return;
  }

  if (auto pd = create_gen_lod_indices_pipeline(info.context, context.compute_local_size_x, false, true)) {
    context.gen_lod_indices_gpu_occlusion_no_cpu_occlusion_pipeline = std::move(pd.value());
  } else {
    return;
  }

  if (auto pd = create_gen_lod_indices_pipeline(info.context, context.compute_local_size_x, false, true, true)) {
    context.gen_lod_indices_gpu_occlusion_no_cpu_occlusion_high_lod_disabled_pipeline = std::move(pd.value());
  } else {
    return;
  }

  if (auto pd = create_partition_lod_indices_pipeline(info.context, context.compute_local_size_x)) {
    context.partition_lod_indices_pipeline = std::move(pd.value());
  } else {
    return;
  }

  if (auto pd = create_gather_newly_disoccluded_indices_pipeline(info.context, context.compute_local_size_x)) {
    context.gather_newly_disoccluded_indices_pipeline = std::move(pd.value());
  } else {
    return;
  }

  const bool no_pcf = context.disable_pcf;
  const bool no_mix = context.disable_color_mix;
  if (auto pd = create_render_forward_pipeline(info.context, false, false, no_pcf, no_mix)) {
    context.render_forwards_array_images_pipeline = std::move(pd.value());
  } else {
    return;
  }

  if (auto pd = create_render_forward_pipeline(info.context, true, false, no_pcf, no_mix)) {
    context.render_forwards_array_images_alpha_to_coverage_pipeline = std::move(pd.value());
  } else {
    return;
  }

  if (auto pd = create_render_forward_pipeline(info.context, false, true, no_pcf, no_mix)) {
    context.render_forwards_mix_color_array_images_pipeline = std::move(pd.value());
  } else {
    return;
  }

  if (auto pd = create_render_forward_pipeline(info.context, false, true, no_pcf, no_mix, true)) {
    context.render_forwards_mix_color_single_channel_alpha_images_pipeline = std::move(pd.value());
  } else {
    return;
  }

  if (auto pd = create_render_forward_pipeline(info.context, true, true, no_pcf, no_mix)) {
    context.render_forwards_mix_color_array_images_alpha_to_coverage_pipeline = std::move(pd.value());
  } else {
    return;
  }

  if (auto pd = create_render_post_forward_pipeline(info.context, false, true, no_pcf, no_mix)) {
    context.render_post_process_mix_color_array_images_pipeline = std::move(pd.value());
  } else {
    return;
  }

  if (auto pd = create_render_shadow_pipeline(info.context)) {
    context.render_shadow_pipeline = std::move(pd.value());
  } else {
    return;
  }

  context.compute_pipelines_valid = true;
}

void init_images(GPUContext& context, const BeginFrameInfo& info) {
  context.hemisphere_color_array_image = create_color_array_image(info, false);
  context.alpha_array_image = create_alpha_test_array_image(info, false);
  context.hemisphere_color_array_image_tiny = create_color_array_image(info, true);
  context.alpha_array_image_tiny = create_alpha_test_array_image(info, true);
  context.single_channel_alpha_array_image_tiny = create_alpha_test_array_image(info, true, true);
  context.mip_mapped_alpha_array_image_tiny = create_mip_mapped_alpha_test_array_image(info);
  context.mip_mapped_hemisphere_color_array_image_tiny = create_mip_mapped_color_array_image(info);
}

void init_transfer_draw_command_buffs(GPUContext& context, const BeginFrameInfo& info) {
  if (!context.transfer_draw_command_buff0.is_valid()) {
    if (auto buff = vk::create_staging_buffer(info.allocator, sizeof(IndirectDrawCommand))) {
      context.transfer_draw_command_buff0 = info.buffer_system.emplace(std::move(buff.value));
    }
  }
  if (!context.transfer_draw_command_buff1.is_valid()) {
    if (auto buff = vk::create_staging_buffer(info.allocator, sizeof(IndirectDrawCommand))) {
      context.transfer_draw_command_buff1 = info.buffer_system.emplace(std::move(buff.value));
    }
  }
}

void lazy_init(GPUContext& context, const BeginFrameInfo& info) {
  init_geometry(context, info);
  init_pipelines(context, info);
  init_images(context, info);
  init_transfer_draw_command_buffs(context, info);
}

void set_instances_modified(
  GPUContext& context, const TreeLeavesRenderData& rd, uint32_t frame_queue_depth) {
  //
  assert(rd.instances_modified);
  for (uint32_t i = 0; i < frame_queue_depth; i++) {
    auto& mod = context.frame_data[i].modified_instances;
    mod.modified = true;
    if (rd.modified_instance_ranges_invalidated) {
      mod.modified_ranges.clear();
      mod.ranges_invalidated = true;
    } else {
      mod.modified_ranges.push(rd.modified_instance_ranges);
    }
  }
}

void set_instance_groups_modified(GPUContext& context, uint32_t frame_queue_depth) {
  for (uint32_t i = 0; i < frame_queue_depth; i++) {
    context.instance_groups_modified[i] = true;
  }
}

void set_cpu_occlusion_frame_data_modified(GPUContext& context, uint32_t frame_queue_depth) {
  for (uint32_t i = 0; i < frame_queue_depth; i++) {
    context.cpu_occlusion_frame_data_modified[i] = true;
  }
}

bool require_draw_indexed_buffers(GPUContext::DrawIndexedBuffers& buffers,
                                  uint32_t reserve_num_instance_indices,
                                  const BeginFrameInfo& info) {
  {
    auto buff = vk::create_device_local_buffer(
      info.allocator, reserve_num_instance_indices * sizeof(DrawInstanceIndex),
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    if (buff) {
      buffers.indices = info.buffer_system.emplace(std::move(buff.value));
    } else {
      return false;
    }
  }

  if (!buffers.indirect_draw_params.is_valid()) {
#ifdef _MSC_VER
    auto use = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
#else //  @NOTE above regarding instance count on macos
    auto use = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
      VK_BUFFER_USAGE_TRANSFER_DST_BIT;
#endif
    auto buff = vk::create_host_visible_buffer(info.allocator, sizeof(IndirectDrawCommand), use);
    if (!buff) {
      return false;
    } else {
      buffers.indirect_draw_params = info.buffer_system.emplace(std::move(buff.value));
    }
  }

  return true;
}

IndirectDrawCommand reset_draw_indexed_buffers(GPUContext::DrawIndexedBuffers& buffers,
                                               uint32_t num_vertex_indices) {
  IndirectDrawCommand prev_written{};
  buffers.indirect_draw_params.get().read(&prev_written, sizeof(IndirectDrawCommand));
  {
    IndirectDrawCommand indirect{};
    indirect.indexCount = num_vertex_indices;
    buffers.indirect_draw_params.get().write(&indirect, sizeof(IndirectDrawCommand));
  }
  return prev_written;
}

void reset_draw_indexed_buffers(GPUContext& context, const BeginFrameInfo& info) {
  assert(context.geometry_buffers);
  const auto& geom = context.geometry_buffers.value();

  auto& fd = context.frame_data[info.frame_index];
  context.prev_written_lod0_indirect_command = reset_draw_indexed_buffers(
    fd.lod0_indices, geom.lod0.num_vertex_indices);
  context.prev_written_lod1_indirect_command = reset_draw_indexed_buffers(
    fd.lod1_indices, geom.lod1.num_vertex_indices);
  //
  context.prev_written_post_forward_lod0_indirect_command = reset_draw_indexed_buffers(
    fd.post_forward_lod0_indices, geom.lod0.num_vertex_indices);
  context.prev_written_post_forward_lod1_indirect_command = reset_draw_indexed_buffers(
    fd.post_forward_lod1_indices, geom.lod1.num_vertex_indices);
}

void update_uniform_buffers(GPUContext& context, const BeginFrameInfo& info) {
  auto& fd = context.frame_data[info.frame_index];
  if (!fd.uniform_buffer.is_valid()) {
    auto buff = vk::create_uniform_buffer(info.allocator, sizeof(RenderForwardsUniformData));
    if (!buff) {
      context.buffers_valid = false;
      return;
    } else {
      fd.uniform_buffer = info.buffer_system.emplace(std::move(buff.value));
    }
  }

  auto un_data = make_render_forwards_uniform_data(
    info.camera,
    info.csm_desc,
    context.render_params.wind_world_bound_xz,
    context.render_params.wind_displacement_limits,
    context.render_params.wind_strength_limits,
    context.render_params.sun_position,
    context.render_params.sun_color);
  fd.uniform_buffer.get().write(&un_data, sizeof(RenderForwardsUniformData));
}

void write_instance_data(
  GPUContext::FrameData& fd, const TreeLeavesRenderData& rd, uint32_t offset, uint32_t count) {
  //
  fd.instances.get().write(
    rd.instances.data() + offset,
    count * sizeof(RenderInstance),
    offset * sizeof(RenderInstance));

  fd.instance_component_indices.get().write(
    rd.instance_component_indices.data() + offset,
    count * sizeof(RenderInstanceComponentIndices),
    offset * sizeof(RenderInstanceComponentIndices));

  fd.lod_compute_instances.get().write(
    rd.compute_lod_instances.data() + offset,
    count * sizeof(ComputeLODInstance),
    offset * sizeof(ComputeLODInstance));
}

bool update_instance_buffers(GPUContext& context, const BeginFrameInfo& info) {
  const uint32_t num_insts = info.render_data->num_instances();
  auto& fd = context.frame_data[info.frame_index];

  uint32_t num_reserved = fd.num_instances_reserved;
  while (num_reserved < num_insts) {
    num_reserved = num_reserved == 0 ? Config::initial_num_instances_reserve : num_reserved * 2;
  }

  bool realloced{};
  if (num_reserved != fd.num_instances_reserved) {
    if (!require_draw_indexed_buffers(fd.lod0_indices, num_reserved, info)) {
      context.buffers_valid = false;
      return false;
    }
    if (!require_draw_indexed_buffers(fd.lod1_indices, num_reserved, info)) {
      context.buffers_valid = false;
      return false;
    }
    if (!require_draw_indexed_buffers(fd.post_forward_lod0_indices, num_reserved, info)) {
      context.buffers_valid = false;
      return false;
    }
    if (!require_draw_indexed_buffers(fd.post_forward_lod1_indices, num_reserved, info)) {
      context.buffers_valid = false;
      return false;
    }

    auto inst_buff = vk::create_storage_buffer(
      info.allocator, num_reserved * sizeof(RenderInstance));
    auto inst_inds_buff = vk::create_storage_buffer(
      info.allocator, num_reserved * sizeof(RenderInstanceComponentIndices));
    auto inst_lod_buff = vk::create_storage_buffer(
      info.allocator, num_reserved * sizeof(ComputeLODInstance));
    auto lod_inds_buff = vk::create_device_local_storage_buffer(
      info.allocator, num_reserved * sizeof(ComputeLODIndex));
    auto lod_dep_buff = vk::create_device_local_storage_buffer(
      info.allocator, num_reserved * sizeof(LODDependentData));
    auto shadow_ind_buff = vk::create_host_visible_vertex_buffer(
      info.allocator, num_reserved * sizeof(uint32_t));

    if (!inst_buff || !inst_inds_buff || !inst_lod_buff ||
        !lod_inds_buff || !lod_dep_buff || !shadow_ind_buff) {
      context.buffers_valid = false;
      return false;
    } else {
      fd.instances = info.buffer_system.emplace(std::move(inst_buff.value));
      fd.instance_component_indices = info.buffer_system.emplace(std::move(inst_inds_buff.value));
      fd.lod_compute_instances = info.buffer_system.emplace(std::move(inst_lod_buff.value));
      fd.computed_lod_indices = info.buffer_system.emplace(std::move(lod_inds_buff.value));
      fd.computed_lod_dependent_data = info.buffer_system.emplace(std::move(lod_dep_buff.value));
      fd.shadow_render_indices = info.buffer_system.emplace(std::move(shadow_ind_buff.value));
      fd.num_instances_reserved = num_reserved;
      realloced = true;
    }
  }

  const auto& mod_insts = fd.modified_instances;
  const bool need_write_all_instances = realloced || mod_insts.ranges_invalidated;

  if (need_write_all_instances) {
    write_instance_data(fd, *info.render_data, 0, num_insts);

  } else {
    for (auto& range : mod_insts.modified_ranges.ranges) {
      write_instance_data(fd, *info.render_data, range.begin, range.end - range.begin);
    }
  }

  //  shadow render indices
  context.cpu_shadow_render_indices.resize(num_insts);
  uint32_t num_shadow_indices{};
  for (uint32_t i = 0; i < num_insts; i++) {
    if (info.render_data->instance_meta[i].enable_fixed_shadow) {
      context.cpu_shadow_render_indices[num_shadow_indices++] = i;
    }
  }
  fd.shadow_render_indices.get().write(
    context.cpu_shadow_render_indices.data(), num_shadow_indices * sizeof(uint32_t));
  fd.num_shadow_instances = num_shadow_indices;

  return true;
}

bool update_instance_group_buffers(GPUContext& context, const BeginFrameInfo& info) {
  const uint32_t num_groups = info.render_data->num_instance_groups();
  auto& fd = context.frame_data[info.frame_index];

  uint32_t num_reserved = fd.num_instance_groups_reserved;
  while (num_reserved < num_groups) {
    num_reserved = num_reserved == 0 ? 128 : num_reserved * 2;
  }
  if (num_reserved != fd.num_instance_groups_reserved) {
    auto groups_buff = vk::create_storage_buffer(
      info.allocator, num_reserved * sizeof(RenderInstanceGroup));
    if (!groups_buff) {
      context.buffers_valid = false;
      return false;
    } else {
      fd.instance_groups = info.buffer_system.emplace(std::move(groups_buff.value));
      fd.num_instance_groups_reserved = num_reserved;
    }
  }

  fd.instance_groups.get().write(
    info.render_data->instance_groups.data(), num_groups * sizeof(RenderInstanceGroup));

  return true;
}

void require_buffers(GPUContext& context, const BeginFrameInfo& info) {
  auto& fd = context.frame_data[info.frame_index];

  auto& mod_insts = context.frame_data[info.frame_index].modified_instances;
  if (mod_insts.modified) {
    fd.num_instances = 0;
    if (update_instance_buffers(context, info)) {
      fd.num_instances = info.render_data->num_instances();
      mod_insts.clear();
    }
  }

  if (context.instance_groups_modified[info.frame_index]) {
    fd.num_instance_groups = 0;
    if (update_instance_group_buffers(context, info)) {
      fd.num_instance_groups = info.render_data->num_instance_groups();
      context.instance_groups_modified[info.frame_index] = false;
    }
  }
}

void update_cpu_occlusion_system_buffers(GPUContext& context, const BeginFrameInfo& info) {
  auto& fd = context.frame_data[info.frame_index];
  { //  clusters
    const uint32_t num_clusters = info.occlusion_system->num_clusters();
    uint32_t num_reserved = fd.num_cpu_occlusion_clusters_reserved;
    while (num_reserved < num_clusters) {
      num_reserved = num_reserved == 0 ? 128 : num_reserved * 2;
    }
    if (num_reserved != fd.num_cpu_occlusion_clusters_reserved) {
      auto buff = vk::create_storage_buffer(
        info.allocator, sizeof(foliage_occlusion::Cluster) * num_reserved);
      if (!buff) {
        context.buffers_valid = false;
        return;
      } else {
        fd.cpu_occlusion_clusters = info.buffer_system.emplace(std::move(buff.value));
        fd.num_cpu_occlusion_clusters_reserved = num_reserved;
      }
    }

    fd.num_cpu_occlusion_clusters = num_clusters;
    if (num_clusters > 0) {
      fd.cpu_occlusion_clusters.get().write(
        info.occlusion_system->clusters.data(),
        num_clusters * sizeof(foliage_occlusion::Cluster));
    }
  }
  { //  cluster group offsets
    const uint32_t num_offs = info.occlusion_system->num_cluster_groups();
    uint32_t num_reserved = fd.num_cpu_occlusion_cluster_group_offsets_reserved;
    while (num_reserved < num_offs) {
      num_reserved = num_reserved == 0 ? 128 : num_reserved * 2;
    }
    if (num_reserved != fd.num_cpu_occlusion_cluster_group_offsets_reserved) {
      auto buff = vk::create_storage_buffer(info.allocator, sizeof(uint32_t) * num_reserved);
      if (!buff) {
        context.buffers_valid = false;
        return;
      } else {
        fd.cpu_occlusion_cluster_group_offsets = info.buffer_system.emplace(std::move(buff.value));
        fd.num_cpu_occlusion_cluster_group_offsets_reserved = num_reserved;
      }
    }

    fd.num_cpu_occlusion_cluster_group_offsets = num_offs;
    if (num_offs > 0) {
      fd.cpu_occlusion_cluster_group_offsets.get().write(
        info.occlusion_system->cluster_group_offsets.data(),
        num_offs * sizeof(uint32_t));
    }
  }
}

void push_gen_lod_indices_common_descriptors(const GPUContext::FrameData& fd,
                                             const BeginFrameInfo& info,
                                             vk::DescriptorSetScaffold& scaffold, uint32_t& bind,
                                             bool is_gpu_occlusion = false) {
  vk::push_storage_buffer(
    scaffold, bind++,
    fd.lod_compute_instances.get(), fd.num_instances * sizeof(ComputeLODInstance));
  vk::push_storage_buffer(
    scaffold, bind++,
    fd.instance_component_indices.get(), fd.num_instances * sizeof(RenderInstanceComponentIndices));
  if (is_gpu_occlusion) {
    auto& prev_info = info.previous_gpu_occlusion_result.value();
    assert(prev_info.num_elements == info.num_frustum_cull_results);
    vk::push_storage_buffer(
      scaffold, bind++, prev_info.result_buffer,
      prev_info.num_elements * sizeof(cull::OcclusionCullAgainstDepthPyramidElementResult));
  } else {
    vk::push_storage_buffer(
      scaffold, bind++,
      info.frustum_cull_results, info.num_frustum_cull_results * sizeof(cull::FrustumCullResult));
  }
  vk::push_storage_buffer(
    scaffold, bind++,
    info.frustum_cull_group_offsets,
    info.num_frustum_cull_group_offsets * sizeof(cull::FrustumCullGroupOffset));
  vk::push_storage_buffer(
    scaffold, bind++,
    fd.computed_lod_indices.get(), fd.num_instances * sizeof(ComputeLODIndex));
  vk::push_storage_buffer(
    scaffold, bind++,
    fd.computed_lod_dependent_data.get(), fd.num_instances * sizeof(LODDependentData));
}

void require_gen_lod_indices_desc_sets(GPUContext& context, const BeginFrameInfo& info) {
  context.gen_lod_desc_set0 = NullOpt{};

  auto& fd = context.frame_data[info.frame_index];
  if (fd.num_instances == 0) {
    return;
  }

  vk::DescriptorSetScaffold scaffold;
  scaffold.set = 0;
  uint32_t bind{};
  push_gen_lod_indices_common_descriptors(fd, info, scaffold, bind);

  auto& pipe = context.gen_lod_indices_pipeline;
  context.gen_lod_desc_set0 = gfx::require_updated_descriptor_set(info.context, scaffold, pipe);
}

void require_gen_lod_indices_cpu_occlusion_desc_sets(GPUContext& context, const BeginFrameInfo& info) {
  context.gen_lod_cpu_occlusion_desc_set0 = NullOpt{};

  auto& fd = context.frame_data[info.frame_index];
  if (fd.num_instances == 0 || fd.num_cpu_occlusion_clusters == 0) {
    return;
  }

  vk::DescriptorSetScaffold scaffold;
  scaffold.set = 0;
  uint32_t bind{};
  push_gen_lod_indices_common_descriptors(fd, info, scaffold, bind);
  vk::push_storage_buffer(
    scaffold, bind++,
    fd.cpu_occlusion_clusters.get(),
    fd.num_cpu_occlusion_clusters * sizeof(foliage_occlusion::Cluster));
  vk::push_storage_buffer(
    scaffold, bind++,
    fd.cpu_occlusion_cluster_group_offsets.get(),
    fd.num_cpu_occlusion_cluster_group_offsets * sizeof(uint32_t));

  auto& pipe = context.gen_lod_indices_cpu_occlusion_pipeline;
  context.gen_lod_cpu_occlusion_desc_set0 = gfx::require_updated_descriptor_set(
    info.context, scaffold, pipe);
}

void require_gen_lod_indices_gpu_occlusion_no_cpu_occlusion_desc_sets(
  GPUContext& context, const BeginFrameInfo& info) {
  //
  context.gen_lod_gpu_occlusion_no_cpu_occlusion_desc_set0 = NullOpt{};

  if (!info.previous_gpu_occlusion_result) {
    return;
  }

  auto& fd = context.frame_data[info.frame_index];
  if (fd.num_instances == 0) {
    return;
  }

  vk::DescriptorSetScaffold scaffold;
  scaffold.set = 0;
  uint32_t bind{};
  push_gen_lod_indices_common_descriptors(fd, info, scaffold, bind, true);

  auto& pipe = context.gen_lod_indices_gpu_occlusion_no_cpu_occlusion_pipeline;
  context.gen_lod_gpu_occlusion_no_cpu_occlusion_desc_set0 =
    gfx::require_updated_descriptor_set(info.context, scaffold, pipe);
}

void require_partition_lod_indices_desc_sets(GPUContext& context, const BeginFrameInfo& info) {
  context.partition_lod0_desc_set0 = NullOpt{};
  context.partition_lod1_desc_set0 = NullOpt{};

  auto& fd = context.frame_data[info.frame_index];
  if (fd.num_instances == 0) {
    return;
  }

  {
    vk::DescriptorSetScaffold scaffold;
    scaffold.set = 0;
    uint32_t bind{};
    vk::push_storage_buffer(
      scaffold, bind++, fd.computed_lod_indices.get(), fd.num_instances * sizeof(ComputeLODIndex));
    vk::push_storage_buffer(
      scaffold, bind++, fd.lod0_indices.indirect_draw_params.get(), sizeof(IndirectDrawCommand));
    vk::push_storage_buffer(
      scaffold, bind++, fd.lod0_indices.indices.get(), fd.num_instances * sizeof(DrawInstanceIndex));

    auto& pipe = context.partition_lod_indices_pipeline;
    context.partition_lod0_desc_set0 = gfx::require_updated_descriptor_set(info.context, scaffold, pipe);
  }
  {
    vk::DescriptorSetScaffold scaffold;
    scaffold.set = 0;
    uint32_t bind{};
    vk::push_storage_buffer(
      scaffold, bind++, fd.computed_lod_indices.get(), fd.num_instances * sizeof(ComputeLODIndex));
    vk::push_storage_buffer(
      scaffold, bind++, fd.lod1_indices.indirect_draw_params.get(), sizeof(IndirectDrawCommand));
    vk::push_storage_buffer(
      scaffold, bind++, fd.lod1_indices.indices.get(), fd.num_instances * sizeof(DrawInstanceIndex));

    auto& pipe = context.partition_lod_indices_pipeline;
    context.partition_lod1_desc_set0 = gfx::require_updated_descriptor_set(info.context, scaffold, pipe);
  }
}

void require_render_shadow_desc_sets(GPUContext& context, const BeginFrameInfo& info) {
  context.render_shadow_desc_set0 = NullOpt{};

  auto& pipe = context.render_shadow_pipeline;
  if (!pipe.is_valid()) {
    return;
  }

  auto& fd = context.frame_data[info.frame_index];
  if (fd.num_instances == 0 || fd.num_shadow_instances == 0) {
    return;
  }

  vk::DescriptorSetScaffold scaffold;
  scaffold.set = 0;

  {
    uint32_t bind{};
    vk::push_storage_buffer(
      scaffold, bind++, fd.instances.get(), fd.num_instances * sizeof(RenderInstance));
    vk::push_storage_buffer(
      scaffold, bind++,
      fd.instance_groups.get(), fd.num_instance_groups * sizeof(RenderInstanceGroup));
  }

  context.render_shadow_desc_set0 = gfx::require_updated_descriptor_set(info.context, scaffold, pipe);
}

void require_render_forward_array_images_desc_sets(GPUContext& context, const BeginFrameInfo& info) {
  context.render_forwards_array_images_desc_set0 = NullOpt{};

  const auto& pipe = context.render_forwards_array_images_pipeline;
  if (!pipe.is_valid()) {
    return;
  }

  auto& fd = context.frame_data[info.frame_index];
  if (fd.num_instances == 0) {
    return;
  }

  if (!context.wind_displacement_image) {
    return;
  }

  bool using_mip_mapped_alpha_image{};
  bool using_mip_mapped_color_image{};

#if 1
  auto alpha_array_image = context.prefer_tiny_array_images ?
    context.alpha_array_image_tiny : context.alpha_array_image;

  if (context.prefer_mip_mapped_images) {
    alpha_array_image = context.mip_mapped_alpha_array_image_tiny;
    using_mip_mapped_alpha_image = true;

  } else if (context.prefer_single_channel_alpha_images) {
    alpha_array_image = context.single_channel_alpha_array_image_tiny;
  }
#else
  auto alpha_array_image = context.alpha_array_image;
#endif
  auto hemisphere_color_array_image = context.prefer_tiny_array_images ?
    context.hemisphere_color_array_image_tiny : context.hemisphere_color_array_image;

  if (context.prefer_mip_mapped_images) {
    hemisphere_color_array_image = context.mip_mapped_hemisphere_color_array_image_tiny;
    using_mip_mapped_color_image = true;
  }

  if (!alpha_array_image || !hemisphere_color_array_image) {
    return;
  }

  Optional<vk::SampledImageManager::ReadInstance> alpha_im;
  if (auto inst = info.sampled_image_manager.get(alpha_array_image.value())) {
    if (inst.value().is_2d_array() &&
        inst.value().fragment_shader_sample_ok() &&
        context.max_instance_alpha_image_index < uint32_t(inst.value().descriptor.shape.depth)) {
      alpha_im = inst.value();
    }
  }

  Optional<vk::SampledImageManager::ReadInstance> color_im;
  if (auto inst = info.sampled_image_manager.get(hemisphere_color_array_image.value())) {
    if (inst.value().is_2d_array() &&
        inst.value().fragment_shader_sample_ok() &&
        context.max_instance_color_image_index < uint32_t(inst.value().descriptor.shape.depth)) {
      color_im = inst.value();
    }
  }

  Optional<vk::DynamicSampledImageManager::ReadInstance> wind_im;
  if (auto inst = info.dynamic_sampled_image_manager.get(context.wind_displacement_image.value())) {
    if (inst.value().is_2d() && inst.value().vertex_shader_sample_ok()) {
      wind_im = inst.value();
    }
  }

  if (!alpha_im || !color_im || !wind_im) {
    return;
  }

  auto sampler = info.sampler_system.require_linear_repeat(info.core.device.handle);
  auto sampler_edge_clamp = info.sampler_system.require_linear_edge_clamp(info.core.device.handle);
  auto alpha_sampler_mip_mapped =
    info.sampler_system.require_linear_edge_clamp_mip_map_nearest(info.core.device.handle);
  auto color_sampler_mip_mapped =
    info.sampler_system.require_linear_repeat_mip_map_nearest(info.core.device.handle);

  vk::DescriptorSetScaffold scaffold;
  scaffold.set = 0;

  {
    auto alpha_sampler = sampler_edge_clamp;
    if (using_mip_mapped_alpha_image) {
      alpha_sampler = alpha_sampler_mip_mapped;
    }

    auto color_sampler = sampler;
    if (using_mip_mapped_color_image) {
      color_sampler = color_sampler_mip_mapped;
    }

    uint32_t bind{};
    vk::push_storage_buffer(
      scaffold, bind++, fd.instances.get(), fd.num_instances * sizeof(RenderInstance));
    vk::push_storage_buffer(
      scaffold, bind++,
      fd.computed_lod_dependent_data.get(), fd.num_instances * sizeof(LODDependentData));
    vk::push_storage_buffer(
      scaffold, bind++,
      fd.instance_groups.get(), fd.num_instance_groups * sizeof(RenderInstanceGroup));
    vk::push_uniform_buffer(
      scaffold, bind++, fd.uniform_buffer.get(), sizeof(RenderForwardsUniformData));
    vk::push_combined_image_sampler(
      scaffold, bind++, wind_im.value().to_sample_image_view(), sampler);
    vk::push_combined_image_sampler(
      scaffold, bind++, alpha_im.value().to_sample_image_view(), alpha_sampler);
    vk::push_combined_image_sampler(
      scaffold, bind++, color_im.value().to_sample_image_view(), color_sampler);
    vk::push_combined_image_sampler(
      scaffold, bind++, info.shadow_image, sampler);
  }

  auto desc_set = gfx::require_updated_descriptor_set(info.context, scaffold, pipe);
  if (desc_set) {
    context.render_forwards_array_images_desc_set0 = desc_set.value();
  }
}

void update_push_constants(GPUContext& context, const BeginFrameInfo& info) {
  context.gen_lod_indices_pc_data = make_gen_lod_indices_push_constant_data(
    context.frame_data[info.frame_index].num_instances, info.camera);

  context.partition_lod_indices_pc_data0 = make_partition_lod_indices_push_constant_data(
    context.frame_data[info.frame_index].num_instances, Config::high_lod_index);
  context.partition_lod_indices_pc_data1 = make_partition_lod_indices_push_constant_data(
    context.frame_data[info.frame_index].num_instances, Config::low_lod_index);

  const float t = context.render_params.prefer_fixed_time ?
    context.render_params.fixed_time : float(info.current_time);
  context.render_forwards_pc_data = make_render_forwards_push_constant_data(info.camera, t);
}

void begin_frame(GPUContext& gpu_context, const BeginFrameInfo& info) {
  if (gpu_context.disabled) {
    return;
  }

  gpu_context.frame_data.resize(int64_t(info.frame_queue_depth));

  if (gpu_context.try_initialize) {
    lazy_init(gpu_context, info);
    gpu_context.try_initialize = false;
  }

  if (gpu_context.set_compute_local_size_x) {
    gpu_context.compute_local_size_x = gpu_context.set_compute_local_size_x.value();
    gpu_context.need_recreate_pipelines = true;
    gpu_context.set_compute_local_size_x = NullOpt{};
  }

  if (gpu_context.need_recreate_pipelines) {
    init_pipelines(gpu_context, info);
    gpu_context.need_recreate_pipelines = false;
  }

#ifdef GROVE_DEBUG
  if (info.render_data->modified_instance_ranges_invalidated ||
      !info.render_data->modified_instance_ranges.empty()) {
    assert(info.render_data->instances_modified);
  }
#endif

  if (info.render_data->instances_modified) {
    set_instances_modified(gpu_context, *info.render_data, info.frame_queue_depth);
    info.render_data->acknowledge_instances_modified();
  }

  if (info.render_data->instance_groups_modified) {
    set_instance_groups_modified(gpu_context, info.frame_queue_depth);
    info.render_data->instance_groups_modified = false;
  }

  if (gpu_context.cpu_occlusion_data_modified) {
    set_cpu_occlusion_frame_data_modified(gpu_context, info.frame_queue_depth);
    gpu_context.cpu_occlusion_data_modified = false;
  }

  require_buffers(gpu_context, info);

  if (gpu_context.cpu_occlusion_frame_data_modified[info.frame_index] && info.occlusion_system) {
    update_cpu_occlusion_system_buffers(gpu_context, info);
    gpu_context.cpu_occlusion_frame_data_modified[info.frame_index] = false;
  }

  update_uniform_buffers(gpu_context, info);

  if (gpu_context.buffers_valid) {
    reset_draw_indexed_buffers(gpu_context, info);
  }

  gpu_context.max_instance_alpha_image_index = info.render_data->max_alpha_image_index;
  gpu_context.max_instance_color_image_index = info.render_data->max_color_image_index;
  gpu_context.num_shadow_instances_drawn = 0;

  require_gen_lod_indices_desc_sets(gpu_context, info);
  require_gen_lod_indices_cpu_occlusion_desc_sets(gpu_context, info);
  require_gen_lod_indices_gpu_occlusion_no_cpu_occlusion_desc_sets(gpu_context, info);
  require_partition_lod_indices_desc_sets(gpu_context, info);
  require_render_forward_array_images_desc_sets(gpu_context, info);
  require_render_shadow_desc_sets(gpu_context, info);
  update_push_constants(gpu_context, info);

  gpu_context.began_frame = true;
}

void end_frame(GPUContext& context) {
  if (context.did_generate_post_forward_draw_indices &&
      context.did_generate_lod_indices_with_gpu_occlusion) {
    context.gui_feedback_did_render_with_gpu_occlusion = true;
  } else {
    context.gui_feedback_did_render_with_gpu_occlusion = false;
  }
  context.began_frame = false;
  context.did_generate_lod_indices_with_gpu_occlusion = false;
  context.did_generate_post_forward_draw_indices = false;
}

void clear_indirect_draw_commands_via_explicit_buffer_copy(
  GPUContext& context, const GPUContext::FrameData& fd, VkCommandBuffer cmd) {
  //
  auto& src_buff0 = context.transfer_draw_command_buff0;
  auto& src_buff1 = context.transfer_draw_command_buff1;
  const auto& geom = context.geometry_buffers;

  if (!src_buff0.is_valid() || !src_buff1.is_valid() || !geom) {
    return;
  }

  IndirectDrawCommand cmd0{};
  cmd0.indexCount = geom.value().lod0.num_vertex_indices;

  IndirectDrawCommand cmd1{};
  cmd1.indexCount = geom.value().lod1.num_vertex_indices;

  src_buff0.get().write(&cmd0, sizeof(IndirectDrawCommand));
  src_buff1.get().write(&cmd1, sizeof(IndirectDrawCommand));

  VkBufferCopy region{};
  region.size = sizeof(IndirectDrawCommand);
  vkCmdCopyBuffer(
    cmd, src_buff0.get().contents().buffer.handle,
    fd.lod0_indices.indirect_draw_params.get().contents().buffer.handle, 1, &region);
  vkCmdCopyBuffer(
    cmd, src_buff0.get().contents().buffer.handle,
    fd.post_forward_lod0_indices.indirect_draw_params.get().contents().buffer.handle, 1, &region);

  vkCmdCopyBuffer(
    cmd, src_buff1.get().contents().buffer.handle,
    fd.lod1_indices.indirect_draw_params.get().contents().buffer.handle, 1, &region);
  vkCmdCopyBuffer(
    cmd, src_buff1.get().contents().buffer.handle,
    fd.post_forward_lod1_indices.indirect_draw_params.get().contents().buffer.handle, 1, &region);

#if 0
  {
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(
      cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
  }
  {
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT |
      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    vkCmdPipelineBarrier(
      cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0,
      1, &barrier, 0, nullptr, 0, nullptr);
  }
#else
  VkMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
  vkCmdPipelineBarrier(
    cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
#endif
}

bool choose_gen_lod_indices_pipeline(
  const GPUContext& context, const gfx::PipelineHandle** pd, VkDescriptorSet* desc_set0) {
  //
  bool using_gpu_occlusion{};
  if (context.prefer_gpu_occlusion &&
      context.gen_lod_indices_gpu_occlusion_no_cpu_occlusion_pipeline.is_valid() &&
      context.gen_lod_indices_gpu_occlusion_no_cpu_occlusion_high_lod_disabled_pipeline.is_valid() &&
      context.gen_lod_gpu_occlusion_no_cpu_occlusion_desc_set0) {

    *desc_set0 = context.gen_lod_gpu_occlusion_no_cpu_occlusion_desc_set0.unwrap();
    if (context.disable_high_lod) {
      *pd = &context.gen_lod_indices_gpu_occlusion_no_cpu_occlusion_high_lod_disabled_pipeline;
    } else {
      *pd = &context.gen_lod_indices_gpu_occlusion_no_cpu_occlusion_pipeline;
    }

    using_gpu_occlusion = true;

  } else {
    if (context.generate_lod_indices_with_cpu_occlusion) {
      *pd = &context.gen_lod_indices_cpu_occlusion_pipeline;
      *desc_set0 = context.gen_lod_cpu_occlusion_desc_set0.unwrap();
    } else {
      *pd = &context.gen_lod_indices_pipeline;
      *desc_set0 = context.gen_lod_desc_set0.unwrap();
    }
  }

  return using_gpu_occlusion;
}

void early_graphics_compute(GPUContext& context, const EarlyComputeInfo& info) {
  if (!context.began_frame || !context.compute_pipelines_valid ||
      !context.buffers_valid || context.disabled) {
    return;
  }
  if (!context.gen_lod_desc_set0 ||
      (context.generate_lod_indices_with_cpu_occlusion && !context.gen_lod_cpu_occlusion_desc_set0) ||
      !context.partition_lod0_desc_set0 ||
      !context.partition_lod1_desc_set0) {
    return;
  }

  auto& fd = context.frame_data[info.frame_index];
  if (fd.num_instances == 0) {
    return;
  }

  if (context.do_clear_indirect_commands_via_explicit_buffer_copy) {
    clear_indirect_draw_commands_via_explicit_buffer_copy(context, fd, info.cmd);
  }

#if 0
  {
    //  Frustum cull
    vk::PipelineBarrierDescriptor barrier_desc{};
    barrier_desc.stages.src = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    barrier_desc.stages.dst = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    VkMemoryBarrier memory_barrier{};
    memory_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memory_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier_desc.num_memory_barriers = 1;
    barrier_desc.memory_barriers = &memory_barrier;

    vk::cmd::pipeline_barrier(info.cmd, &barrier_desc);
  }
#endif

  const auto tot_loc_size = context.compute_local_size_x;
  const auto num_dispatch = std::ceil(double(fd.num_instances) / tot_loc_size);

  {
    auto profiler = GROVE_VK_SCOPED_DEBUG_LABEL(info.cmd, "gen_lod_indices");
    (void) profiler;

    const gfx::PipelineHandle* pd;
    VkDescriptorSet desc_set0;
    context.did_generate_lod_indices_with_gpu_occlusion =
      choose_gen_lod_indices_pipeline(context, &pd, &desc_set0);

    vk::cmd::bind_compute_descriptor_sets(info.cmd, pd->get_layout(), 0, 1, &desc_set0);
    vk::cmd::bind_compute_pipeline(info.cmd, pd->get());
    vk::cmd::push_constants(
      info.cmd, pd->get_layout(), VK_SHADER_STAGE_COMPUTE_BIT, &context.gen_lod_indices_pc_data);
    vkCmdDispatch(info.cmd, uint32_t(num_dispatch), 1, 1);
  }

  {
    //  Gen lod indices
    vk::PipelineBarrierDescriptor barrier_desc{};
    barrier_desc.stages.src = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    barrier_desc.stages.dst = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    VkMemoryBarrier memory_barrier{};
    memory_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memory_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier_desc.num_memory_barriers = 1;
    barrier_desc.memory_barriers = &memory_barrier;

    vk::cmd::pipeline_barrier(info.cmd, &barrier_desc);
  }

  {
    auto profiler = GROVE_VK_SCOPED_DEBUG_LABEL(info.cmd, "partition_lod_indices0");
    (void) profiler;

    auto& pd = context.partition_lod_indices_pipeline;
    vk::cmd::bind_compute_descriptor_sets(
      info.cmd, pd.get_layout(), 0, 1, &context.partition_lod0_desc_set0.value());
    vk::cmd::bind_compute_pipeline(info.cmd, pd.get());
    vk::cmd::push_constants(
      info.cmd, pd.get_layout(), VK_SHADER_STAGE_COMPUTE_BIT, &context.partition_lod_indices_pc_data0);

    vkCmdDispatch(info.cmd, uint32_t(num_dispatch), 1, 1);
  }
  {
    auto profiler = GROVE_VK_SCOPED_DEBUG_LABEL(info.cmd, "partition_lod_indices1");
    (void) profiler;

    auto& pd = context.partition_lod_indices_pipeline;
    vk::cmd::bind_compute_descriptor_sets(
      info.cmd, pd.get_layout(), 0, 1, &context.partition_lod1_desc_set0.value());
    vk::cmd::bind_compute_pipeline(info.cmd, pd.get());
    vk::cmd::push_constants(
      info.cmd, pd.get_layout(), VK_SHADER_STAGE_COMPUTE_BIT, &context.partition_lod_indices_pc_data1);

    vkCmdDispatch(info.cmd, uint32_t(num_dispatch), 1, 1);
  }
  {
    //  render
    vk::PipelineBarrierDescriptor barrier_desc{};
    barrier_desc.stages.src = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    barrier_desc.stages.dst = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;

    VkMemoryBarrier memory_barrier{};
    memory_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memory_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memory_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
    barrier_desc.num_memory_barriers = 1;
    barrier_desc.memory_barriers = &memory_barrier;

    vk::cmd::pipeline_barrier(info.cmd, &barrier_desc);
  }
}

void post_forward_graphics_compute(GPUContext& context, const PostForwardComputeInfo& info) {
  if (!context.began_frame || !context.did_generate_lod_indices_with_gpu_occlusion ||
      !info.current_gpu_occlusion_result || !info.frustum_cull_group_offsets ||
      context.post_forward_compute_disabled) {
    return;
  }

  auto& pipe = context.gather_newly_disoccluded_indices_pipeline;
  if (!pipe.is_valid()) {
    return;
  }

  auto db_label = GROVE_VK_SCOPED_DEBUG_LABEL(info.cmd, "tree_leaves_gather_newly_disoccluded_indices");
  (void) db_label;

  auto& fd = context.frame_data[info.frame_index];

  auto& occlusion_info = info.current_gpu_occlusion_result.value();
  vk::DescriptorSetScaffold scaffold;
  scaffold.set = 0;
  uint32_t bind{};
  vk::push_storage_buffer(
    scaffold, bind++, occlusion_info.result_buffer,
    occlusion_info.num_elements * sizeof(cull::OcclusionCullAgainstDepthPyramidElementResult));
  vk::push_storage_buffer(
    scaffold, bind++,
    fd.computed_lod_indices.get(), fd.num_instances * sizeof(ComputeLODIndex));
  vk::push_storage_buffer(
    scaffold, bind++,
    fd.instance_component_indices.get(), fd.num_instances * sizeof(RenderInstanceComponentIndices));
  vk::push_storage_buffer(
    scaffold, bind++,
    *info.frustum_cull_group_offsets,
    info.num_frustum_cull_group_offsets * sizeof(cull::FrustumCullGroupOffset));
  //  out
  vk::push_storage_buffer(
    scaffold, bind++,
    fd.post_forward_lod0_indices.indices.get(), fd.num_instances * sizeof(uint32_t));
  vk::push_storage_buffer(
    scaffold, bind++,
    fd.post_forward_lod0_indices.indirect_draw_params.get(), sizeof(IndirectDrawCommand));
  vk::push_storage_buffer(
    scaffold, bind++,
    fd.post_forward_lod1_indices.indices.get(), fd.num_instances * sizeof(uint32_t));
  vk::push_storage_buffer(
    scaffold, bind++,
    fd.post_forward_lod1_indices.indirect_draw_params.get(), sizeof(IndirectDrawCommand));

  auto desc_set = gfx::require_updated_descriptor_set(info.context, scaffold, pipe);
  if (!desc_set) {
    return;
  }

  vk::cmd::bind_compute_pipeline(info.cmd, pipe.get());
  vk::cmd::bind_compute_descriptor_sets(info.cmd, pipe.get_layout(), 0, 1, &desc_set.value());

  GatherNewlyDisoccludedIndicesPushConstantData pc{};
  pc.num_instances_unused = Vec4<uint32_t>{fd.num_instances, 0, 0, 0};
  vk::cmd::push_constants(info.cmd, pipe.get_layout(), VK_SHADER_STAGE_COMPUTE_BIT, &pc);

  const auto tot_loc_size = context.compute_local_size_x;
  const auto num_dispatch = std::ceil(double(fd.num_instances) / tot_loc_size);
  vkCmdDispatch(info.cmd, uint32_t(num_dispatch), 1, 1);

  {
    vk::PipelineBarrierDescriptor barrier_desc{};
    barrier_desc.stages.src = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    barrier_desc.stages.dst = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;

    VkMemoryBarrier memory_barrier{};
    memory_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memory_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memory_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
    barrier_desc.num_memory_barriers = 1;
    barrier_desc.memory_barriers = &memory_barrier;

    vk::cmd::pipeline_barrier(info.cmd, &barrier_desc);
  }

  context.did_generate_post_forward_draw_indices = true;
}

void draw_forward(const TreeLeavesRenderForwardInfo& info,
                  const GPUContext::GeometryBuffers& geom,
                  const GPUContext::DrawIndexedBuffers& lod0_indices,
                  const GPUContext::DrawIndexedBuffers& lod1_indices) {
  {
    VkBuffer vert_buffs[2]{
      geom.lod0.geometry.get(),
      lod0_indices.indices.get().contents().buffer.handle
    };
    VkDeviceSize vb_offs[2]{};
    VkBuffer ind_buff = geom.lod0.indices.get();

    VkBuffer indirect_buff = lod0_indices.indirect_draw_params.get().contents().buffer.handle;

    vkCmdBindIndexBuffer(info.cmd, ind_buff, 0, VK_INDEX_TYPE_UINT16);
    vkCmdBindVertexBuffers(info.cmd, 0, 2, vert_buffs, vb_offs);
    vkCmdDrawIndexedIndirect(info.cmd, indirect_buff, 0, 1, 0);
  }
  {
    VkBuffer vert_buffs[2]{
      geom.lod1.geometry.get(),
      lod1_indices.indices.get().contents().buffer.handle
    };
    VkDeviceSize vb_offs[2]{};
    VkBuffer ind_buff = geom.lod1.indices.get();

    VkBuffer indirect_buff = lod1_indices.indirect_draw_params.get().contents().buffer.handle;
    vkCmdBindIndexBuffer(info.cmd, ind_buff, 0, VK_INDEX_TYPE_UINT16);
    vkCmdBindVertexBuffers(info.cmd, 0, 2, vert_buffs, vb_offs);
    vkCmdDrawIndexedIndirect(info.cmd, indirect_buff, 0, 1, 0);
  }
}

void render_forward(GPUContext& context, const RenderForwardInfo& info,
                    const gfx::PipelineHandle& pd, VkDescriptorSet desc_set0,
                    const GPUContext::DrawIndexedBuffers& lod0_indices,
                    const GPUContext::DrawIndexedBuffers& lod1_indices) {
  if (!context.began_frame || context.disabled || context.forward_rendering_disabled ||
      !pd.is_valid() || !context.buffers_valid || !context.geometry_buffers) {
    return;
  }

  auto& fd = context.frame_data[info.frame_index];
  if (fd.num_instances == 0) {
    return;
  }

  auto profiler = GROVE_VK_SCOPED_DEBUG_LABEL(info.cmd, "render_new_tree_leaves_forward");
  (void) profiler;

  vk::cmd::bind_graphics_pipeline(info.cmd, pd.get());
  vk::cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor_rect);
  vk::cmd::bind_graphics_descriptor_sets(info.cmd, pd.get_layout(), 0, 1, &desc_set0);

  auto pc_stages = VK_SHADER_STAGE_VERTEX_BIT;
  vk::cmd::push_constants(info.cmd, pd.get_layout(), pc_stages, &context.render_forwards_pc_data);

  draw_forward(info, context.geometry_buffers.value(), lod0_indices, lod1_indices);
}

void render_forward(GPUContext& context, const RenderForwardInfo& info) {
  const gfx::PipelineHandle* ph{};
  auto desc_set0 = context.render_forwards_array_images_desc_set0;

  if (!desc_set0) {
    return;
  }

  if (context.render_forward_with_color_image_mix) {
    if (context.render_forward_with_alpha_to_coverage) {
      ph = &context.render_forwards_mix_color_array_images_alpha_to_coverage_pipeline;
    } else {
      if (context.prefer_single_channel_alpha_images && !context.prefer_mip_mapped_images) {
        ph = &context.render_forwards_mix_color_single_channel_alpha_images_pipeline;
      } else {
        ph = &context.render_forwards_mix_color_array_images_pipeline;
      }
    }
  } else {
    if (context.render_forward_with_alpha_to_coverage) {
      ph = &context.render_forwards_array_images_alpha_to_coverage_pipeline;
    } else {
      ph = &context.render_forwards_array_images_pipeline;
    }
  }

  auto& fd = context.frame_data[info.frame_index];
  render_forward(context, info, *ph, desc_set0.value(), fd.lod0_indices, fd.lod1_indices);
}

void render_post_process(GPUContext& context, const RenderForwardInfo& info) {
  if (!context.did_generate_post_forward_draw_indices) {
    return;
  }

  auto& pipe = context.render_post_process_mix_color_array_images_pipeline;
  if (!pipe.is_valid()) {
    return;
  }

  auto desc_set0 = context.render_forwards_array_images_desc_set0;
  if (!desc_set0) {
    return;
  }

  auto& fd = context.frame_data[info.frame_index];
  render_forward(
    context, info, pipe, desc_set0.value(),
    fd.post_forward_lod0_indices, fd.post_forward_lod1_indices);
}

void render_shadow(GPUContext& context, const TreeLeavesRenderShadowInfo& info) {
  const auto& pd = context.render_shadow_pipeline;

  if (!context.began_frame || context.disabled || context.shadow_rendering_disabled ||
      !pd.is_valid() || !context.buffers_valid || !context.geometry_buffers) {
    return;
  }
  if (!context.render_shadow_desc_set0) {
    return;
  }

  auto& fd = context.frame_data[info.frame_index];
  if (fd.num_instances == 0 || fd.num_shadow_instances == 0) {
    return;
  }

  if (info.cascade_index > context.max_shadow_cascade_index) {
    return;
  }

  auto profiler = GROVE_VK_SCOPED_DEBUG_LABEL(info.cmd, "render_new_tree_leaves_shadow");
  (void) profiler;

  vk::cmd::bind_graphics_pipeline(info.cmd, pd.get());
  vk::cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor_rect);
  vk::cmd::bind_graphics_descriptor_sets(
    info.cmd, pd.get_layout(), 0, 1, &context.render_shadow_desc_set0.value());

  //  @NOTE: Same push constant data for now.
  auto pc_stages = VK_SHADER_STAGE_VERTEX_BIT;
  auto pc_data = context.render_forwards_pc_data;
  pc_data.projection_view = info.proj_view;
  vk::cmd::push_constants(info.cmd, pd.get_layout(), pc_stages, &pc_data);

  const auto& geom = context.geometry_buffers.value();
  {
    VkBuffer vert_buffs[2]{
      geom.lod1.geometry.get(),
      fd.shadow_render_indices.get().contents().buffer.handle
    };
    VkDeviceSize vb_offs[2]{};
    VkBuffer ind_buff = geom.lod1.indices.get();

    vkCmdBindVertexBuffers(info.cmd, 0, 2, vert_buffs, vb_offs);
    vkCmdBindIndexBuffer(info.cmd, ind_buff, 0, VK_INDEX_TYPE_UINT16);

    vk::DrawIndexedDescriptor draw_desc;
    draw_desc.num_instances = fd.num_shadow_instances;
    draw_desc.num_indices = geom.lod1.num_vertex_indices;
    vk::cmd::draw_indexed(info.cmd, &draw_desc);
  }

  context.num_shadow_instances_drawn = fd.num_shadow_instances;
}

struct {
  GPUContext context;
} globals;

} //  anon

void foliage::tree_leaves_renderer_render_forward(const TreeLeavesRenderForwardInfo& info) {
  render_forward(globals.context, info);
}

void foliage::tree_leaves_renderer_render_post_process(const TreeLeavesRenderForwardInfo& info) {
  render_post_process(globals.context, info);
}

void foliage::tree_leaves_renderer_render_shadow(const TreeLeavesRenderShadowInfo& info) {
  render_shadow(globals.context, info);
}

void foliage::tree_leaves_renderer_set_cpu_occlusion_data_modified() {
  globals.context.cpu_occlusion_data_modified = true;
}

void foliage::tree_leaves_renderer_begin_frame(const TreeLeavesRendererBeginFrameInfo& info) {
  begin_frame(globals.context, info);
}

void foliage::tree_leaves_renderer_end_frame() {
  end_frame(globals.context);
}

void foliage::tree_leaves_renderer_early_graphics_compute(const EarlyComputeInfo& info) {
  early_graphics_compute(globals.context, info);
}

void foliage::tree_leaves_renderer_post_forward_graphics_compute(const PostForwardComputeInfo& info) {
  post_forward_graphics_compute(globals.context, info);
}

TreeLeavesRenderParams* foliage::get_tree_leaves_render_params() {
  return &globals.context.render_params;
}

TreeLeavesRendererStats foliage::get_tree_leaves_renderer_stats() {
  const auto& context = globals.context;
  TreeLeavesRendererStats result{};

  uint32_t num_lod0_vertex_indices{};
  uint32_t num_lod1_vertex_indices{};
  if (context.geometry_buffers) {
    auto& geom = context.geometry_buffers.value();
    num_lod0_vertex_indices = geom.lod0.num_vertex_indices;
    num_lod1_vertex_indices = geom.lod1.num_vertex_indices;
  }

  {
    auto num_lod0 = context.prev_written_lod0_indirect_command.instanceCount;
    auto num_lod1 = context.prev_written_lod1_indirect_command.instanceCount;
    result.prev_num_lod0_forward_instances = num_lod0;
    result.prev_num_lod1_forward_instances = num_lod1;
    result.prev_total_num_forward_instances = num_lod0 + num_lod1;
    result.prev_num_forward_vertices_drawn =
      context.prev_written_lod0_indirect_command.instanceCount * num_lod0_vertex_indices +
      context.prev_written_lod1_indirect_command.instanceCount * num_lod1_vertex_indices;
  }
  {
    auto num_lod0 = context.prev_written_post_forward_lod0_indirect_command.instanceCount;
    auto num_lod1 = context.prev_written_post_forward_lod1_indirect_command.instanceCount;
    result.prev_num_lod0_post_forward_instances = num_lod0;
    result.prev_num_lod1_post_forward_instances = num_lod1;
    result.prev_total_num_post_forward_instances = num_lod0 + num_lod1;
    result.prev_num_post_forward_vertices_drawn =
      context.prev_written_post_forward_lod0_indirect_command.instanceCount * num_lod0_vertex_indices +
      context.prev_written_post_forward_lod1_indirect_command.instanceCount * num_lod1_vertex_indices;
  }
  result.num_shadow_instances = context.num_shadow_instances_drawn;
  result.did_render_with_gpu_occlusion = context.gui_feedback_did_render_with_gpu_occlusion;
  return result;
}

bool foliage::get_tree_leaves_renderer_forward_rendering_enabled() {
  return !globals.context.forward_rendering_disabled;
}
void foliage::set_tree_leaves_renderer_forward_rendering_enabled(bool enabled) {
  globals.context.forward_rendering_disabled = !enabled;
}
bool foliage::get_tree_leaves_renderer_enabled() {
  return !globals.context.disabled;
}
void foliage::set_tree_leaves_renderer_enabled(bool enabled) {
  globals.context.disabled = !enabled;
}

bool foliage::get_tree_leaves_renderer_use_tiny_array_images() {
  return globals.context.prefer_tiny_array_images;
}

void foliage::set_tree_leaves_renderer_use_tiny_array_images(bool v) {
  globals.context.prefer_tiny_array_images = v;
}

bool foliage::get_tree_leaves_renderer_use_alpha_to_coverage() {
  return globals.context.render_forward_with_alpha_to_coverage;
}

void foliage::set_tree_leaves_renderer_use_alpha_to_coverage(bool v) {
  globals.context.render_forward_with_alpha_to_coverage = v;
}

void foliage::set_tree_leaves_renderer_cpu_occlusion_enabled(bool v) {
  globals.context.generate_lod_indices_with_cpu_occlusion = v;
}

bool foliage::get_tree_leaves_renderer_cpu_occlusion_enabled() {
  return globals.context.generate_lod_indices_with_cpu_occlusion;
}

uint32_t foliage::get_tree_leaves_renderer_max_shadow_cascade_index() {
  return globals.context.max_shadow_cascade_index;
}

void foliage::set_tree_leaves_renderer_max_shadow_cascade_index(uint32_t ind) {
  globals.context.max_shadow_cascade_index = ind;
}

void foliage::set_tree_leaves_renderer_wind_displacement_image(uint32_t image_handle_id) {
  globals.context.wind_displacement_image = vk::DynamicSampledImageManager::Handle{image_handle_id};
}

bool foliage::get_set_tree_leaves_renderer_prefer_color_image_mix_pipeline(const bool* v) {
  if (v) {
    globals.context.render_forward_with_color_image_mix = *v;
  }
  return globals.context.render_forward_with_color_image_mix;
}

bool foliage::get_set_tree_leaves_renderer_shadow_rendering_disabled(const bool* v) {
  if (v) {
    globals.context.shadow_rendering_disabled = *v;
  }
  return globals.context.shadow_rendering_disabled;
}

bool foliage::get_set_tree_leaves_renderer_prefer_gpu_occlusion(const bool* v) {
  if (v) {
    globals.context.prefer_gpu_occlusion = *v;
  }
  return globals.context.prefer_gpu_occlusion;
}

bool foliage::get_set_tree_leaves_renderer_post_forward_graphics_compute_disabled(const bool* v) {
  if (v) {
    globals.context.post_forward_compute_disabled = *v;
  }
  return globals.context.post_forward_compute_disabled;
}

bool foliage::get_set_tree_leaves_renderer_pcf_disabled(const bool* v) {
  if (v) {
    globals.context.disable_pcf = *v;
    globals.context.need_recreate_pipelines = true;
  }
  return globals.context.disable_pcf;
}

bool foliage::get_set_tree_leaves_renderer_color_mix_disabled(const bool* v) {
  if (v) {
    globals.context.disable_color_mix = *v;
    globals.context.need_recreate_pipelines = true;
  }
  return globals.context.disable_color_mix;
}

bool foliage::get_set_tree_leaves_renderer_use_mip_mapped_images(const bool* v) {
  if (v) {
    globals.context.prefer_mip_mapped_images = *v;
  }
  return globals.context.prefer_mip_mapped_images;
}

bool foliage::get_set_tree_leaves_renderer_use_single_channel_alpha_images(const bool* v) {
  if (v) {
    globals.context.prefer_single_channel_alpha_images = *v;
  }
  return globals.context.prefer_single_channel_alpha_images;
}

bool foliage::get_set_tree_leaves_renderer_do_clear_indirect_commands_via_explicit_buffer_copy(const bool* v) {
  if (v) {
    globals.context.do_clear_indirect_commands_via_explicit_buffer_copy = *v;
  }
  return globals.context.do_clear_indirect_commands_via_explicit_buffer_copy;
}

int foliage::get_set_tree_leaves_renderer_compute_local_size_x(const int* x) {
  if (x && *x > 0 && (*x & (*x - 1)) == 0) {
    globals.context.set_compute_local_size_x = *x;
  }
  return globals.context.compute_local_size_x;
}

bool foliage::get_set_tree_leaves_renderer_disable_high_lod(const bool* v) {
  if (v) {
    globals.context.disable_high_lod = *v;
  }
  return globals.context.disable_high_lod;
}

void foliage::recreate_tree_leaves_renderer_pipelines() {
  globals.context.need_recreate_pipelines = true;
}

void foliage::terminate_tree_leaves_renderer() {
  globals.context = {};
}

GROVE_NAMESPACE_END
