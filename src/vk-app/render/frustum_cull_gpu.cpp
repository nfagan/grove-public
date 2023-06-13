#include "frustum_cull_gpu.hpp"
#include "frustum_cull_data.hpp"
#include "debug_frustum_cull.hpp"
#include "../vk/vk.hpp"
#include "./debug_label.hpp"
#include "graphics.hpp"
#include "grove/math/Vec3.hpp"
#include "grove/math/Frustum.hpp"
#include "grove/common/common.hpp"
#include <bitset>

GROVE_NAMESPACE_BEGIN

namespace {

using namespace cull;

using EarlyInfo = FrustumCullGPUContextEarlyGraphicsComputeInfo;

constexpr uint32_t max_num_data_sets = 4;

struct FrameDataSet {
  bool is_valid{};

  vk::BufferSystem::BufferHandle instances;
  vk::BufferSystem::BufferHandle group_offsets;

  uint32_t num_instances_reserved{};
  uint32_t num_instances{};
  uint32_t num_group_offsets_reserved{};
  uint32_t num_group_offsets{};

  vk::BufferSystem::BufferHandle outputs[max_num_data_sets];
  Frustum output_frustums[max_num_data_sets];
  uint32_t num_outputs{};
};

struct DescriptorSets {
  Optional<VkDescriptorSet> desc_set0s[max_num_data_sets];
};

struct ReadResults {
  Optional<GPUReadFrustumCullResults> results[max_num_data_sets];
};

struct FrustumCullGPUContext {
  uint32_t num_data_sets{};
  DynamicArray<FrameDataSet, 3> frame_data_sets[max_num_data_sets];
  std::bitset<32> frame_data_sets_modified[max_num_data_sets]{};
  DescriptorSets desc_set0_sets[max_num_data_sets];
  ReadResults result_sets[max_num_data_sets];

  gfx::PipelineHandle pipeline_handle;

  int compute_local_size_x{32};

  float camera_far{512.0f};

  bool debug_draw_enabled{};
  bool try_initialize{true};
};

struct PushConstantData {
  Vec4<uint32_t> num_instances_unused;
  Vec4f near;
  Vec4f far;
  Vec4f left;
  Vec4f right;
  Vec4f top;
  Vec4f bottom;
};

PushConstantData make_push_constant_data(const Frustum& frustum, uint32_t num_instances) {
  PushConstantData result;
  result.num_instances_unused = Vec4<uint32_t>{num_instances, 0, 0, 0};
  result.near = frustum.planes.near;
  result.far = frustum.planes.far;
  result.left = frustum.planes.left;
  result.right = frustum.planes.right;
  result.top = frustum.planes.top;
  result.bottom = frustum.planes.bottom;
  return result;
}

void set_modified(std::bitset<32>& bs, uint32_t frame_queue_depth) {
  for (uint32_t i = 0; i < frame_queue_depth; i++) {
    bs[i] = true;
  }
}

Optional<gfx::PipelineHandle> create_pipeline(gfx::Context& context, int local_size_x) {
  glsl::LoadComputeProgramSourceParams params{};
  params.file = "cull/frustum-cull.comp";

  params.compile.defines.push_back(glsl::make_integer_define("LOCAL_SIZE_X", local_size_x));
  params.compile.defines.push_back(glsl::make_integer_define("LOCAL_SIZE_Y", 1));
  auto src = glsl::make_compute_program_source(params);
  if (!src) {
    return NullOpt{};
  }

  return gfx::create_compute_pipeline(&context, std::move(src.value()));
}

void lazy_init(FrustumCullGPUContext* context, const FrustumCullGPUContextBeginFrameInfo& info) {
  if (auto pd = create_pipeline(info.context, context->compute_local_size_x)) {
    context->pipeline_handle = std::move(pd.value());
  }
}

GPUReadFrustumCullResults make_read_result(FrameDataSet& ds, uint32_t oi) {
  assert(oi < ds.num_outputs);
  GPUReadFrustumCullResults read{};
  read.results = &ds.outputs[oi].get();
  read.num_results = ds.num_instances;
  read.group_offsets = &ds.group_offsets.get();
  read.num_group_offsets = ds.num_group_offsets;
  read.instances = &ds.instances.get();
  return read;
}

void update_buffers(const FrustumCullInputs& src, FrameDataSet& dst,
                    const FrustumCullGPUContextBeginFrameInfo& info) {
  dst.num_instances = 0;
  dst.num_group_offsets = 0;
  dst.is_valid = false;

  const uint32_t num_insts = src.cpu_cull_data->num_instances();
  const uint32_t num_group_offs = src.cpu_cull_data->num_group_offsets();

  if (num_insts == 0) {
    return;
  }

  {
    //  instances
    uint32_t num_reserved = dst.num_instances_reserved;
    while (num_reserved < num_insts) {
      num_reserved = num_reserved == 0 ? 64 : num_reserved * 2;
    }
    if (num_reserved != dst.num_instances_reserved) {
      dst.num_outputs = src.num_frustums;

      auto inst_buff = vk::create_storage_buffer(
        info.allocator, num_reserved * sizeof(FrustumCullInstance));
      if (!inst_buff) {
        return;
      } else {
        dst.instances = info.buffer_system.emplace(std::move(inst_buff.value));
      }

      assert(src.num_frustums <= max_num_data_sets);
      for (uint32_t i = 0; i < src.num_frustums; i++) {
        auto res_buff = vk::create_storage_buffer(
          info.allocator, num_reserved * sizeof(FrustumCullResult));
        if (!res_buff) {
          return;
        } else {
          dst.outputs[i] = info.buffer_system.emplace(std::move(res_buff.value));
        }
      }

      dst.num_instances_reserved = num_reserved;

    } else {
      assert(dst.num_outputs == src.num_frustums);
    }

    if (num_insts > 0) {
      dst.instances.get().write(
        src.cpu_cull_data->instances.data(), num_insts * sizeof(FrustumCullInstance));
    }
  }
  {
    //  group offsets
    uint32_t num_reserved = dst.num_group_offsets_reserved;
    while (num_reserved < num_group_offs) {
      num_reserved = num_reserved == 0 ? 16 : num_reserved * 2;
    }
    if (num_reserved != dst.num_group_offsets_reserved) {
      auto buff = vk::create_storage_buffer(
        info.allocator, num_reserved * sizeof(FrustumCullGroupOffset));
      if (!buff) {
        return;
      } else {
        dst.group_offsets = info.buffer_system.emplace(std::move(buff.value));
        dst.num_group_offsets_reserved = num_reserved;
      }
    }

    if (num_group_offs > 0) {
      dst.group_offsets.get().write(
        src.cpu_cull_data->group_offsets.data(), num_group_offs * sizeof(FrustumCullGroupOffset));
    }
  }

  dst.num_instances = num_insts;
  dst.num_group_offsets = num_group_offs;
  dst.is_valid = true;
}

Optional<VkDescriptorSet> prepare_desc_set0(
  FrustumCullGPUContext* context, FrameDataSet& ds, uint32_t output_index,
  const FrustumCullGPUContextBeginFrameInfo& info) {
  //
  assert(output_index < max_num_data_sets);

  auto& pipe = context->pipeline_handle;
  if (!pipe.is_valid()) {
    return NullOpt{};
  }

  if (ds.num_instances == 0) {
    return NullOpt{};
  }

  vk::DescriptorSetScaffold scaffold;
  scaffold.set = 0;
  uint32_t bind{};
  vk::push_storage_buffer(
    scaffold, bind++, ds.instances.get(), ds.num_instances * sizeof(FrustumCullInstance));
  vk::push_storage_buffer(
    scaffold, bind++, ds.outputs[output_index].get(), ds.num_instances * sizeof(FrustumCullResult));

  return gfx::require_updated_descriptor_set(&info.context, scaffold, pipe);
}

cull::FrustumCullGPUContextBeginFrameResult begin_frame(
  FrustumCullGPUContext* context, const FrustumCullGPUContextBeginFrameInfo& info) {
  //
  cull::FrustumCullGPUContextBeginFrameResult result{};

  if (context->try_initialize) {
    lazy_init(context, info);
    context->try_initialize = false;
  }

  assert(info.num_cull_inputs <= max_num_data_sets);
  for (uint32_t i = 0; i < info.num_cull_inputs; i++) {
    auto& input_set = info.cull_inputs[i];

    auto& frame_data_set = context->frame_data_sets[i];
    while (info.frame_index >= uint32_t(frame_data_set.size())) {
      frame_data_set.emplace_back();
    }

    auto& dst_set = frame_data_set[info.frame_index];

    assert(input_set.num_frustums < max_num_data_sets);
    for (uint32_t j = 0; j < input_set.num_frustums; j++) {
      dst_set.output_frustums[j] = input_set.arg_frustums[j];
    }

    if (input_set.cpu_cull_data->groups_added_or_removed) {
      //  When groups are added, cached occlusion results from the previous frame won't have data
      //  for the new instances; the buffer might also be smaller than required to fit the current
      //  number of instances. When groups are removed, results from the previous frame might no
      //  longer correspond to the new locations of the (potentially) moved instances.
      assert(input_set.cpu_cull_data->modified);
      input_set.cpu_cull_data->groups_added_or_removed = false;
      result.dependent_instances_potentially_invalidated[i] = true;
    }

    if (input_set.cpu_cull_data->modified) {
      input_set.cpu_cull_data->modified = false;
      set_modified(context->frame_data_sets_modified[i], info.frame_queue_depth);
    }

    if (context->frame_data_sets_modified[i][info.frame_index]) {
      update_buffers(input_set, dst_set, info);
      context->frame_data_sets_modified[i][info.frame_index] = false;
    }

    for (auto& desc_set0 : context->desc_set0_sets[i].desc_set0s) {
      desc_set0 = NullOpt{};
    }

    if (dst_set.is_valid) {
      assert(dst_set.num_outputs == input_set.num_frustums);
      for (uint32_t j = 0; j < dst_set.num_outputs; j++) {
        context->desc_set0_sets[i].desc_set0s[j] = prepare_desc_set0(context, dst_set, j, info);
      }
    }

    for (auto& read_res : context->result_sets[i].results) {
      read_res = NullOpt{};
    }

    if (context->pipeline_handle.is_valid() && dst_set.is_valid) {
      for (uint32_t j = 0; j < dst_set.num_outputs; j++) {
        if (context->desc_set0_sets[i].desc_set0s[j]) {
          context->result_sets[i].results[j] = make_read_result(dst_set, j);
        }
      }
    }
  }

  context->num_data_sets = info.num_cull_inputs;
  return result;
}

void early_graphics_compute(FrustumCullGPUContext* context, const EarlyInfo& info) {
  auto& pipe = context->pipeline_handle;

  if (!pipe.is_valid()) {
    return;
  }

  auto profiler = GROVE_VK_SCOPED_DEBUG_LABEL(info.cmd, "frustum_cull_compute");
  (void) profiler;

  vk::cmd::bind_compute_pipeline(info.cmd, pipe.get());

  for (uint32_t i = 0; i < context->num_data_sets; i++) {
    auto& fd = context->frame_data_sets[i][info.frame_index];
    if (fd.num_instances == 0) {
      continue;
    }

    for (uint32_t j = 0; j < fd.num_outputs; j++) {
      auto& desc_set0 = context->desc_set0_sets[i].desc_set0s[j];
      if (!desc_set0) {
        continue;
      }

      vk::cmd::bind_compute_descriptor_sets(info.cmd, pipe.get_layout(), 0, 1, &desc_set0.value());

      auto pc_data = make_push_constant_data(fd.output_frustums[j], fd.num_instances);
      vk::cmd::push_constants(info.cmd, pipe.get_layout(), VK_SHADER_STAGE_COMPUTE_BIT, &pc_data);

      auto num_dispatch = std::ceil(double(fd.num_instances) / double(context->compute_local_size_x));
      vkCmdDispatch(info.cmd, uint32_t(num_dispatch), 1, 1);
    }
  }

#if 1
  {
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
}

void update(FrustumCullGPUContext* ctx, const FrustumCullData** datas, uint32_t count) {
  if (ctx->debug_draw_enabled) {
    for (uint32_t i = 0; i < count; i++) {
      auto color = i == 0 ? Vec3f{} : Vec3f{1.0f, 0.0f, 0.0f};
      debug::draw_frustum_cull_data(datas[i], color);
    }
  }
}

struct {
  FrustumCullGPUContext context;
} globals;

} //  anon

void cull::frustum_cull_gpu_context_update(const FrustumCullData** cull_datas, uint32_t count) {
  grove::update(&globals.context, cull_datas, count);
}

cull::FrustumCullGPUContextBeginFrameResult
cull::frustum_cull_gpu_context_begin_frame(const FrustumCullGPUContextBeginFrameInfo& info) {
  return grove::begin_frame(&globals.context, info);
}

void cull::terminate_frustum_cull_gpu_context() {
  globals.context = {};
}

void cull::frustum_cull_gpu_context_early_graphics_compute(const EarlyInfo& info) {
  grove::early_graphics_compute(&globals.context, info);
}

Optional<GPUReadFrustumCullResults>
cull::frustum_cull_gpu_context_read_results(uint32_t input, uint32_t output) {
  assert(input < max_num_data_sets && output < max_num_data_sets);
  return globals.context.result_sets[input].results[output];
}

float cull::get_frustum_cull_far_plane_distance() {
  return globals.context.camera_far;
}

void cull::set_frustum_cull_far_plane_distance(float d) {
  globals.context.camera_far = d;
}

void cull::set_frustum_cull_debug_draw_enabled(bool enable) {
  globals.context.debug_draw_enabled = enable;
}

bool cull::get_frustum_cull_debug_draw_enabled() {
  return globals.context.debug_draw_enabled;
}

GROVE_NAMESPACE_END
