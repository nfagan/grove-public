#pragma once

#include "grove/math/vector.hpp"
#include <vulkan/vulkan.h>

namespace grove {
template <typename T>
struct Mat4;
class Camera;
}

namespace grove::gfx {
struct Context;
}

namespace grove::csm {
struct CSMDescriptor;
}

namespace grove::vk {
class SampledImageManager;
class DynamicSampledImageManager;
struct SampleImageView;
}

namespace grove::foliage {

struct OrnamentalFoliageData;

struct RenderOrnamentalFoliageBeginFrameInfo {
  gfx::Context* graphics_context;
  uint32_t frame_index;
  uint32_t frame_queue_depth;
  OrnamentalFoliageData* cpu_data;
  vk::SampledImageManager* sampled_image_manager;
  vk::DynamicSampledImageManager* dynamic_sampled_image_manager;
  const csm::CSMDescriptor& csm_desc;
  const vk::SampleImageView& shadow_image;
  const Camera& camera;
};

struct RenderOrnamentalFoliageRenderForwardInfo {
  VkCommandBuffer cmd;
  VkViewport viewport;
  VkRect2D scissor;
  uint32_t frame_index;
  const Camera& camera;
};

struct RenderOrnamentalFoliageRenderParams {
  Vec3f sun_position;
  Vec3f sun_color;
  Vec4f wind_world_bound_xz;
  Vec2f wind_displacement_limits;
  Vec2f wind_strength_limits;
  float elapsed_time;
  float branch_elapsed_time;
};

struct RenderOrnamentalFoliageStats {
  uint32_t num_curved_plane_small_instances;
  uint32_t num_curved_plane_large_instances;
  uint32_t num_flat_plane_small_instances;
  uint32_t num_flat_plane_large_instances;
  bool wrote_to_instance_buffers;
  bool wrote_to_indices_buffers;
};

void render_ornamental_foliage_begin_frame(const RenderOrnamentalFoliageBeginFrameInfo& info);
void render_ornamental_foliage_render_forward(const RenderOrnamentalFoliageRenderForwardInfo& info);
void terminate_ornamental_foliage_rendering();

RenderOrnamentalFoliageRenderParams* get_render_ornamental_foliage_render_params();
RenderOrnamentalFoliageStats get_render_ornamental_foliage_stats();

void set_render_ornamental_foliage_wind_displacement_image(uint32_t id);
bool get_render_ornamental_foliage_disabled();
void set_render_ornamental_foliage_disabled(bool disable);

int get_render_ornamental_foliage_num_material1_texture_layers();

}