#pragma once

#include "grove/math/vector.hpp"
#include "grove/common/Optional.hpp"
#include <vulkan/vulkan.h>

namespace grove {
template <typename T>
struct Mat4;
class Camera;
}

namespace grove::csm {
struct CSMDescriptor;
}

namespace grove::gfx {
struct Context;
}

namespace grove::vk {
class DynamicSampledImageManager;
struct SampleImageView;
}

namespace grove::tree {

struct RenderBranchNodesData;

struct RenderBranchNodesBeginFrameInfo {
  gfx::Context* graphics_context;
  RenderBranchNodesData* cpu_data;
  uint32_t frame_queue_depth;
  uint32_t frame_index;
  vk::DynamicSampledImageManager* dynamic_sampled_image_manager;
  const Camera& camera;
  const csm::CSMDescriptor& csm_desc;
  const vk::SampleImageView& shadow_image;
};

struct RenderBranchNodesCullResults {
  VkBuffer results_buffer;
  size_t num_results;
  VkBuffer group_offsets_buffer;
  size_t num_group_offsets;
};

struct RenderBranchNodesEarlyGraphicsComputeInfo {
  gfx::Context* context;
  uint32_t frame_index;
  VkCommandBuffer cmd;
  Optional<RenderBranchNodesCullResults> frustum_cull_results;
  Optional<RenderBranchNodesCullResults> occlusion_cull_results;
};

struct RenderBranchNodesRenderForwardInfo {
  uint32_t frame_index;
  VkCommandBuffer cmd;
  VkViewport viewport;
  VkRect2D scissor;
  const Camera& camera;
};

struct RenderBranchNodesRenderShadowInfo {
  uint32_t frame_index;
  VkCommandBuffer cmd;
  VkViewport viewport;
  VkRect2D scissor;
  uint32_t cascade_index;
  const Mat4<float>& proj_view;
};

struct RenderBranchNodesRenderParams {
  float elapsed_time;
  Vec2f wind_displacement_limits;
  Vec2f wind_strength_limits;
  Vec4f wind_world_bound_xz;
  Vec3f sun_position;
  Vec3f sun_color;
  uint32_t max_num_instances;
  bool limit_to_max_num_instances;
};

struct RenderBranchNodesStats {
  uint32_t prev_num_base_forward_instances;
  uint32_t prev_num_wind_forward_instances;
  bool rendered_base_forward_with_occlusion_culling;
  bool rendered_wind_forward_with_occlusion_culling;
};

void render_branch_nodes_begin_frame(const RenderBranchNodesBeginFrameInfo& info);
void render_branch_nodes_end_frame();
void render_branch_nodes_early_graphics_compute(const RenderBranchNodesEarlyGraphicsComputeInfo& info);
void render_branch_nodes_forward(const RenderBranchNodesRenderForwardInfo& info);
void render_branch_nodes_shadow(const RenderBranchNodesRenderShadowInfo& info);
void terminate_branch_node_renderer();
void set_render_branch_nodes_wind_displacement_image(uint32_t id);

void set_render_branch_nodes_disabled(bool disable);
bool get_render_branch_nodes_disabled();

bool get_set_render_branch_nodes_prefer_cull_enabled(const bool* enable);
bool get_set_render_branch_nodes_base_shadow_disabled(const bool* disabled);
bool get_set_render_branch_nodes_wind_shadow_disabled(const bool* disabled);
bool get_set_render_branch_nodes_disable_base_drawables(const bool* enable);
bool get_set_render_branch_nodes_disable_wind_drawables(const bool* enable);
bool get_set_render_branch_nodes_prefer_low_lod_geometry(const bool* pref);
bool get_set_render_branch_nodes_render_base_drawables_as_quads(const bool* pref);
bool get_set_render_branch_nodes_render_wind_drawables_as_quads(const bool* pref);
uint32_t get_set_render_branch_nodes_max_cascade_index(const uint32_t* ind);

RenderBranchNodesRenderParams* get_render_branch_nodes_render_params();

RenderBranchNodesStats get_render_branch_nodes_stats();

}