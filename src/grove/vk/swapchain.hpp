#pragma once

#include "image.hpp"
#include <vector>

namespace grove::vk {

struct PhysicalDevice;
struct Device;
struct Surface;
struct FramebufferDimensions;

struct Swapchain {
  uint32_t num_image_views() const {
    return uint32_t(image_views.size());
  }

  VkSwapchainKHR handle{VK_NULL_HANDLE};
  VkFormat image_format{};
  VkExtent2D extent{};
  VkPresentModeKHR present_mode{};
  std::vector<VkImage> images;
  std::vector<ImageView> image_views;
};

Result<Swapchain> create_swapchain(const PhysicalDevice& physical_device,
                                   const Device& device,
                                   const Surface& surface,
                                   const FramebufferDimensions& fb_dims);
void destroy_swapchain(Swapchain* swapchain, VkDevice device);
const char* to_string(VkPresentModeKHR present_mode);

}