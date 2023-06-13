#pragma once

#include "../vk/vk.hpp"

namespace grove::vk {

struct ShadowPass {
  size_t approx_image_memory_usage() const;
  SampleImageView make_sample_image_view() const {
    SampleImageView result{};
    result.view = array_view.handle;
    result.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    return result;
  }

  RenderPass render_pass;
  std::vector<Framebuffer> framebuffers;
  ManagedImage image;
  std::vector<ImageView> sub_views;
  ImageView array_view;
  VkFormat format{};
  VkExtent2D extent{};
  VkSampleCountFlagBits raster_samples{};
};

struct CreateShadowPassInfo {
  VkDevice device;
  Allocator* allocator;
  VkFormat depth_format;
  uint32_t image_dim;
  uint32_t num_layers;
  VkSampleCountFlagBits samples;
};

Optional<VkFormat> choose_shadow_pass_image_format(const PhysicalDevice& device);

Result<ShadowPass> create_shadow_pass(const CreateShadowPassInfo* info);
void destroy_shadow_pass(ShadowPass* pass, VkDevice device);

}