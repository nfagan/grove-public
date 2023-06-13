#pragma once

#include "grove/math/vector.hpp"
#include <vulkan/vulkan.h>

namespace grove {
class Camera;
}

namespace grove::gfx {
struct Context;
}

namespace grove::vk {
class DynamicSampledImageManager;
struct PipelineRenderPassInfo;
}

namespace grove::tree {

struct RenderVineSystem;

struct VineRenderNode {
  Vec4f self_position_radius;
  Vec4f child_position_radius;
  Vec4<uint32_t> directions0;
  Vec4<uint32_t> directions1;
  Vec4<uint32_t> self_aggregate_index_child_aggregate_index_unused;
  Vec4<uint32_t> wind_info0;
  Vec4<uint32_t> wind_info1;
  Vec4<uint32_t> wind_info2;
};

struct VineAttachedToAggregateRenderData {
  Vec4f wind_aabb_p0;
  Vec4f wind_aabb_p1;
};

struct RenderVinesBeginFrameInfo {
  gfx::Context* graphics_context;
  vk::DynamicSampledImageManager* dynamic_sampled_image_manager;
  const vk::PipelineRenderPassInfo& forward_pass_info;
  RenderVineSystem* render_vine_system;
  uint32_t frame_index;
  uint32_t frame_queue_depth;
};

struct RenderVinesForwardRenderInfo {
  gfx::Context* graphics_context;
  VkCommandBuffer cmd;
  VkRect2D scissor;
  VkViewport viewport;
  const Camera& camera;
  uint32_t frame_index;
};

void render_vines_begin_frame(const RenderVinesBeginFrameInfo& info);
void render_vines_forward(const RenderVinesForwardRenderInfo& info);
void terminate_vine_renderer();
void set_render_vines_wind_displacement_image(uint32_t handle_id);
void set_render_vines_need_remake_programs();
void set_render_vines_elapsed_time(float t);
void set_render_vines_wind_info(const Vec4f& wind_world_bound_xz,
                                const Vec2f& wind_displacement_limits,
                                const Vec2f& wind_strength_limits);
Vec3f get_render_vines_color();
void set_render_vines_color(const Vec3f& c);

}