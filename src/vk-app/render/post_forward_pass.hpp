#pragma once

#include "../vk/vk.hpp"

namespace grove::vk {

struct PostForwardPass {
  RenderPass render_pass;
  Framebuffer framebuffer;
};

struct PostForwardPassCreateInfo {
  VkDevice device;
  VkImageView single_sample_color_image_view;
  VkImageView single_sample_depth_image_view;
  VkFormat color_format;
  VkFormat depth_format;
  VkExtent2D image_extent;
};

Result<PostForwardPass> create_post_forward_pass(const PostForwardPassCreateInfo* info);
void destroy_post_forward_pass(PostForwardPass* pass, VkDevice device);

}