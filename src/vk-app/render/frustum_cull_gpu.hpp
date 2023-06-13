#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>
#include "grove/math/Frustum.hpp"

namespace grove {
template <typename T>
struct Vec3;

template <typename T>
class Optional;

class Camera;
}

namespace grove::gfx {
struct Context;
}

namespace grove::vk {
class BufferSystem;
class Allocator;
class ManagedBuffer;
struct Core;
}

namespace grove::cull {

struct FrustumCullData;

struct FrustumCullInputs {
  FrustumCullData* cpu_cull_data;
  Frustum arg_frustums[2];
  uint32_t num_frustums;
};

struct FrustumCullGPUContextBeginFrameInfo {
  const FrustumCullInputs* cull_inputs;
  uint32_t num_cull_inputs;
  gfx::Context& context;
  uint32_t frame_index;
  uint32_t frame_queue_depth;
  const vk::Core& core;
  vk::Allocator* allocator;
  vk::BufferSystem& buffer_system;
};

struct FrustumCullGPUContextBeginFrameResult {
  bool dependent_instances_potentially_invalidated[4];
};

struct FrustumCullGPUContextEarlyGraphicsComputeInfo {
  VkCommandBuffer cmd;
  uint32_t frame_index;
};

struct GPUReadFrustumCullResults {
  const vk::ManagedBuffer* instances; //  `num_results` instances
  const vk::ManagedBuffer* results;
  uint32_t num_results;
  const vk::ManagedBuffer* group_offsets;
  uint32_t num_group_offsets;
};

void frustum_cull_gpu_context_update(const FrustumCullData** cull_datas, uint32_t num_cull_datas);

[[nodiscard]]
FrustumCullGPUContextBeginFrameResult
frustum_cull_gpu_context_begin_frame(const FrustumCullGPUContextBeginFrameInfo& info);

void frustum_cull_gpu_context_early_graphics_compute(const FrustumCullGPUContextEarlyGraphicsComputeInfo& info);
void terminate_frustum_cull_gpu_context();

Optional<GPUReadFrustumCullResults>
frustum_cull_gpu_context_read_results(uint32_t input, uint32_t output);

float get_frustum_cull_far_plane_distance();
void set_frustum_cull_far_plane_distance(float d);

void set_frustum_cull_debug_draw_enabled(bool enable);
bool get_frustum_cull_debug_draw_enabled();

}