#pragma once

#include "../vk/vk.hpp"

namespace grove::vk {

struct ForwardWriteBackPass {
  bool multisampling_enabled() const {
    return image_samples != VK_SAMPLE_COUNT_1_BIT;
  }
  vk::SampleImageView make_sample_color_image_view() const;
  vk::SampleImageView make_sample_depth_image_view() const;
  VkImage get_single_sample_depth_image() const;
  size_t approx_image_memory_usage() const;

  RenderPass render_pass;
  Framebuffer framebuffer;
  ManagedImage multisample_color_image;
  ManagedImageView multisample_color_image_view;
  ManagedImage multisample_depth_image;
  ManagedImageView multisample_depth_image_view;
  ManagedImage single_sample_color_image;
  ManagedImageView single_sample_color_image_view;
  ManagedImage single_sample_depth_image;
  ManagedImageView single_sample_depth_image_view;
  VkFormat color_image_format{};
  VkFormat depth_image_format{};
  VkSampleCountFlagBits image_samples{};
  VkExtent2D image_extent{};
};

struct ForwardWriteBackPassCreateInfo {
  VkInstance instance;
  VkDevice device;
  Allocator* allocator;
  VkFormat color_format;
  VkFormat depth_format;
  VkExtent2D image_extent;
  VkSampleCountFlagBits image_samples;
  VkResolveModeFlagBits depth_resolve_mode;
};

VkSampleCountFlagBits choose_forward_write_back_pass_samples(const PhysicalDevice& device,
                                                             int num_samples);
Optional<VkResolveModeFlagBits>
choose_forward_write_back_pass_depth_resolve_mode(const PhysicalDevice& device);
Optional<VkFormat> choose_forward_write_back_pass_depth_format(VkPhysicalDevice device);

Result<ForwardWriteBackPass>
create_forward_write_back_pass(const ForwardWriteBackPassCreateInfo* info);
void destroy_forward_write_back_pass(ForwardWriteBackPass* pass, VkDevice device);

}