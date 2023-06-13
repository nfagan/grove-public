#pragma once

#include "../vk/vk.hpp"

namespace grove::vk {

struct PostProcessPass {
  size_t approx_image_memory_usage() const;
  vk::SampleImageView make_sample_color_image_view() const;

  RenderPass render_pass;
  std::vector<Framebuffer> framebuffers;
  ManagedImage maybe_color_image;
  ManagedImageView maybe_color_image_view;
  ManagedImage depth_image;
  ManagedImageView depth_image_view;
  VkFormat color_image_format{};
  VkFormat depth_image_format{};
  VkSampleCountFlagBits raster_samples{};
  VkExtent2D image_extent{};
};

struct PostProcessPassCreateInfo {
  bool separate_present_pass_enabled;
  VkDevice device;
  Allocator* allocator;
  const VkImageView* present_image_views;
  uint32_t num_present_image_views;
  VkFormat color_format;
  VkFormat depth_format;
  VkExtent2D image_extent;
};

Optional<VkFormat> choose_post_process_pass_depth_format(VkPhysicalDevice device);

Result<PostProcessPass> create_post_process_pass(const PostProcessPassCreateInfo* info);
void destroy_post_process_pass(PostProcessPass* pass, VkDevice device);

}