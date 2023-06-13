#pragma once

#include "../vk/vk.hpp"

namespace grove::vk {

Error create_depth_image_components(
  VkDevice device, Allocator* allocator, VkFormat format, VkExtent2D extent,
  ManagedImage* out_image, ManagedImageView* out_view);

Error create_framebuffers_with_one_color_attachment(
  VkDevice device, const VkImageView* color_views, uint32_t num_color_views,
  VkImageView depth_image, VkExtent2D extent, VkRenderPass render_pass,
  vk::Framebuffer* out_framebuffers);

Error create_attachment_image_and_view(
  VkDevice device, Allocator* allocator, VkFormat format, uint32_t width, uint32_t height,
  VkImageUsageFlags usage, VkSampleCountFlagBits samples, VkImageAspectFlags aspect,
  ManagedImage* out_image, ManagedImageView* out_view);

}