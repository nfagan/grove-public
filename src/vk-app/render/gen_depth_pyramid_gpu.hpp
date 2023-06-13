#pragma once

#include "../vk/vk.hpp"

namespace grove::gfx {
struct Context;
}

namespace grove::vk {
struct GraphicsContext;
}

namespace grove::gpu {

struct GenDepthPyramidInfo {
  gfx::Context& context;
  vk::GraphicsContext& vk_context;
  Optional<vk::SampleImageView> sample_scene_depth_image;
  VkExtent2D scene_image_extent;
  VkCommandBuffer cmd;
  uint32_t frame_index;
};

struct GenDepthPyramidResult {
  Optional<vk::SampleImageView> sample_depth_pyramid;
  VkExtent2D depth_pyramid_image_extent;
  uint32_t depth_pyramid_image_num_mips;
};

[[nodiscard]] GenDepthPyramidResult gen_depth_pyramid(const GenDepthPyramidInfo& info);
void terminate_gen_depth_pyramid();
bool get_set_gen_depth_pyramid_enabled(const bool* v);

}