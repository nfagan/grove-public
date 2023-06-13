#pragma once

#include "grove/math/vector.hpp"
#include "grove/math/Mat4.hpp"
#include "grove/common/Optional.hpp"
#include <cstdint>
#include <vulkan/vulkan.h>

namespace grove::csm {
struct CSMDescriptor;
}

namespace grove {
class Camera;
}

namespace grove::gfx {
struct Context;
}

namespace grove::vk {
struct PipelineRenderPassInfo;
class PipelineSystem;
class BufferSystem;
class DescriptorSystem;
class SamplerSystem;
class Allocator;
struct Core;
class ManagedBuffer;
class SampledImageManager;
class DynamicSampledImageManager;
struct SampleImageView;
class CommandProcessor;
}

namespace grove::foliage_occlusion {
struct FoliageOcclusionSystem;
}

namespace grove::foliage {

struct TreeLeavesRenderData;

struct TreeLeavesRendererStats {
  uint32_t num_shadow_instances;
  uint32_t prev_num_lod0_forward_instances;
  uint32_t prev_num_lod1_forward_instances;
  uint32_t prev_total_num_forward_instances;
  uint32_t prev_num_forward_vertices_drawn;

  uint32_t prev_num_lod0_post_forward_instances;
  uint32_t prev_num_lod1_post_forward_instances;
  uint32_t prev_total_num_post_forward_instances;
  uint32_t prev_num_post_forward_vertices_drawn;

  bool did_render_with_gpu_occlusion;
};

struct TreeLeavesRendererGPUOcclusionCullResult {
  VkBuffer result_buffer;
  size_t num_elements;
};

struct TreeLeavesRendererBeginFrameInfo {
  gfx::Context* context;
  TreeLeavesRenderData* render_data;
  const foliage_occlusion::FoliageOcclusionSystem* occlusion_system;
  uint32_t frame_index;
  uint32_t frame_queue_depth;
  vk::Allocator* allocator;
  const vk::Core& core;
  vk::BufferSystem& buffer_system;
  vk::PipelineSystem& pipeline_system;
  vk::DescriptorSystem& descriptor_system;
  vk::SamplerSystem& sampler_system;
  vk::CommandProcessor& command_processor;
  vk::SampledImageManager& sampled_image_manager;
  const vk::DynamicSampledImageManager& dynamic_sampled_image_manager;
  const vk::ManagedBuffer& frustum_cull_results;
  uint32_t num_frustum_cull_results;
  const vk::ManagedBuffer& frustum_cull_group_offsets;
  uint32_t num_frustum_cull_group_offsets;
  const Camera& camera;
  const csm::CSMDescriptor& csm_desc;
  const vk::PipelineRenderPassInfo& forward_render_pass_info;
  const vk::PipelineRenderPassInfo& shadow_render_pass_info;
  double current_time;
  const vk::SampleImageView& shadow_image;
  Optional<TreeLeavesRendererGPUOcclusionCullResult> previous_gpu_occlusion_result;
};

struct TreeLeavesRendererEarlyGraphicsComputeInfo {
  VkCommandBuffer cmd;
  uint32_t frame_index;
};

struct TreeLeavesRendererPostForwardGraphicsComputeInfo {
  gfx::Context* context;
  VkCommandBuffer cmd;
  uint32_t frame_index;
  Optional<TreeLeavesRendererGPUOcclusionCullResult> current_gpu_occlusion_result;
  const vk::ManagedBuffer* frustum_cull_group_offsets;  //  can be null
  uint32_t num_frustum_cull_group_offsets;
};

struct TreeLeavesRenderForwardInfo {
  VkCommandBuffer cmd;
  uint32_t frame_index;
  VkViewport viewport;
  VkRect2D scissor_rect;
};

struct TreeLeavesRenderShadowInfo {
  VkCommandBuffer cmd;
  uint32_t frame_index;
  uint32_t cascade_index;
  VkViewport viewport;
  VkRect2D scissor_rect;
  const Mat4f& proj_view;
};

struct TreeLeavesRenderParams {
  Vec3f sun_position;
  Vec3f sun_color;
  Vec4f wind_world_bound_xz;
  Vec2f wind_displacement_limits;
  Vec2f wind_strength_limits;
  float global_color_image_mix;
  float fixed_time;
  bool prefer_fixed_time;
};

void tree_leaves_renderer_begin_frame(const TreeLeavesRendererBeginFrameInfo& info);
void tree_leaves_renderer_end_frame();
void tree_leaves_renderer_early_graphics_compute(const TreeLeavesRendererEarlyGraphicsComputeInfo& info);
void tree_leaves_renderer_post_forward_graphics_compute(const TreeLeavesRendererPostForwardGraphicsComputeInfo& info);
void tree_leaves_renderer_render_forward(const TreeLeavesRenderForwardInfo& info);
void tree_leaves_renderer_render_post_process(const TreeLeavesRenderForwardInfo& info);
void tree_leaves_renderer_render_shadow(const TreeLeavesRenderShadowInfo& info);
void tree_leaves_renderer_set_cpu_occlusion_data_modified();
void terminate_tree_leaves_renderer();

void set_tree_leaves_renderer_wind_displacement_image(uint32_t image_handle_id);

TreeLeavesRenderParams* get_tree_leaves_render_params();
bool get_tree_leaves_renderer_forward_rendering_enabled();
void set_tree_leaves_renderer_forward_rendering_enabled(bool enabled);
bool get_tree_leaves_renderer_enabled();
void set_tree_leaves_renderer_enabled(bool enabled);
bool get_tree_leaves_renderer_use_tiny_array_images();
void set_tree_leaves_renderer_use_tiny_array_images(bool v);
bool get_set_tree_leaves_renderer_use_mip_mapped_images(const bool* v);
bool get_set_tree_leaves_renderer_use_single_channel_alpha_images(const bool* v);
bool get_tree_leaves_renderer_use_alpha_to_coverage();
void set_tree_leaves_renderer_use_alpha_to_coverage(bool v);
bool get_tree_leaves_renderer_cpu_occlusion_enabled();
void set_tree_leaves_renderer_cpu_occlusion_enabled(bool v);
bool get_set_tree_leaves_renderer_prefer_color_image_mix_pipeline(const bool* v);
bool get_set_tree_leaves_renderer_prefer_gpu_occlusion(const bool* v);
bool get_set_tree_leaves_renderer_shadow_rendering_disabled(const bool* v);
bool get_set_tree_leaves_renderer_post_forward_graphics_compute_disabled(const bool* v);
bool get_set_tree_leaves_renderer_pcf_disabled(const bool* v);
bool get_set_tree_leaves_renderer_color_mix_disabled(const bool* v);
bool get_set_tree_leaves_renderer_do_clear_indirect_commands_via_explicit_buffer_copy(const bool* v);
bool get_set_tree_leaves_renderer_disable_high_lod(const bool* v);
int get_set_tree_leaves_renderer_compute_local_size_x(const int* x);
uint32_t get_tree_leaves_renderer_max_shadow_cascade_index();
void set_tree_leaves_renderer_max_shadow_cascade_index(uint32_t ind);
void recreate_tree_leaves_renderer_pipelines();

TreeLeavesRendererStats get_tree_leaves_renderer_stats();

}