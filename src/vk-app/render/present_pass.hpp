#pragma once

#include "../vk/vk.hpp"

namespace grove::vk {

struct PresentPass {
  size_t approx_image_memory_usage() const;

  RenderPass render_pass;
  std::vector<Framebuffer> framebuffers;
  ManagedImage depth_image;
  ManagedImageView depth_image_view;
  VkFormat color_image_format{};
  VkFormat depth_image_format{};
  VkSampleCountFlagBits raster_samples{};
};

struct PresentPassCreateInfo {
  VkDevice device;
  Allocator* allocator;
  const VkImageView* present_image_views;
  uint32_t num_present_image_views;
  VkFormat color_format;
  VkFormat depth_format;
  VkExtent2D image_extent;
};

Optional<VkFormat> choose_present_pass_depth_format(VkPhysicalDevice device);

Result<PresentPass> create_present_pass(const PresentPassCreateInfo* info);
void destroy_present_pass(PresentPass* pass, VkDevice device);

}