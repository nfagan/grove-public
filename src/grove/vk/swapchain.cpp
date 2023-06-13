#include "swapchain.hpp"
#include "physical_device.hpp"
#include "device.hpp"
#include "surface.hpp"
#include "grove/common/common.hpp"
#include "grove/common/scope.hpp"
#include <GLFW/glfw3.h>

GROVE_NAMESPACE_BEGIN

namespace {

std::vector<VkImage> get_swapchain_images(VkDevice device, VkSwapchainKHR swapchain) {
  uint32_t count{};
  vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr);
  std::vector<VkImage> result(count);
  vkGetSwapchainImagesKHR(device, swapchain, &count, result.data());
  return result;
}

VkExtent2D clamp_swap_extent(uint32_t fb_width,
                             uint32_t fb_height,
                             const VkSurfaceCapabilitiesKHR& cap) {
  if (cap.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
    return cap.currentExtent;
  } else {
    VkExtent2D extent{fb_width, fb_height};
    extent.width = std::max(cap.minImageExtent.width,
                            std::min(cap.maxImageExtent.width, extent.width));
    extent.height = std::max(cap.minImageExtent.height,
                             std::min(cap.maxImageExtent.height, extent.height));
    return extent;
  }
}

VkImageViewCreateInfo make_swap_surface_image_view_create_info(VkImage image, VkFormat format) {
  VkImageViewCreateInfo view_create_info{};
  view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_create_info.image = image;
  view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_create_info.format = format;
  //
  view_create_info.components = vk::make_identity_component_mapping();
  //
  view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  view_create_info.subresourceRange.baseMipLevel = 0;
  view_create_info.subresourceRange.levelCount = 1;
  view_create_info.subresourceRange.baseArrayLayer = 0;
  view_create_info.subresourceRange.layerCount = 1;
  return view_create_info;
}

vk::Result<vk::Swapchain> create_swapchain(VkDevice device,
                                           const VkSwapchainCreateInfoKHR* create_info,
                                           VkFormat surface_format,
                                           VkExtent2D extent,
                                           VkPresentModeKHR present_mode) {
  vk::Swapchain result{};
  result.image_format = surface_format;
  result.extent = extent;
  result.present_mode = present_mode;

  bool success = false;
  GROVE_SCOPE_EXIT {
    if (!success) {
      vk::destroy_swapchain(&result, device);
    }
  };

  auto create_res = vkCreateSwapchainKHR(device, create_info, GROVE_VK_ALLOC, &result.handle);
  if (create_res != VK_SUCCESS) {
    return {create_res, "Failed to create swap chain."};
  }

  result.images = get_swapchain_images(device, result.handle);
  for (auto& im : result.images) {
    auto view_create_info = make_swap_surface_image_view_create_info(im, surface_format);
    auto im_res = vk::create_image_view(device, &view_create_info);
    if (!im_res) {
      return vk::error_cast<vk::Swapchain>(im_res);
    } else {
      result.image_views.push_back(im_res.value);
    }
  }

  success = true;
  return result;
}

vk::Result<int> pick_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& formats,
                                         VkFormat desired_format,
                                         VkColorSpaceKHR desired_color_space) {
  const auto match = [desired_format, desired_color_space](const VkSurfaceFormatKHR& format) {
    return format.format == desired_format && format.colorSpace == desired_color_space;
  };
  if (auto it = std::find_if(formats.begin(), formats.end(), match); it != formats.end()) {
    return int(it - formats.begin());
  } else {
    return {VK_ERROR_INITIALIZATION_FAILED, "No surface formats met requirements."};
  }
}

vk::Result<int> pick_swap_present_mode(const std::vector<VkPresentModeKHR>& available_modes,
                                       const std::vector<VkPresentModeKHR>& preferred_modes) {
  for (auto& mode : preferred_modes) {
    if (auto it = std::find(available_modes.begin(), available_modes.end(), mode);
        it != available_modes.end()) {
      return int(it - available_modes.begin());
    }
  }
  return {VK_ERROR_INITIALIZATION_FAILED, "No present modes met requirements."};
}

} //  anon

vk::Result<vk::Swapchain> vk::create_swapchain(const PhysicalDevice& physical_device,
                                               const Device& device,
                                               const Surface& surface,
                                               const FramebufferDimensions& fb_dims) {
  assert(physical_device.rendering_supported());
  const auto swapchain_info = get_swapchain_support_info(
    physical_device.handle, surface.handle);

  VkSurfaceFormatKHR surface_format{};
  const VkFormat desired_format = VK_FORMAT_B8G8R8A8_SRGB;
//  const VkFormat desired_format = VK_FORMAT_B8G8R8A8_UNORM;
//  const VkFormat desired_format = VK_FORMAT_R16G16B16A16_SFLOAT;
  const VkColorSpaceKHR desired_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

  auto swap_ind = pick_swap_surface_format(
    swapchain_info.formats, desired_format, desired_color_space);
  if (swap_ind) {
    surface_format = swapchain_info.formats[swap_ind.value];
  } else {
    return error_cast<Swapchain>(swap_ind);
  }

  //  @TODO: Revisit preferred present modes.
  VkPresentModeKHR present_mode{};
  std::vector<VkPresentModeKHR> preferred_modes{
    VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_MAILBOX_KHR
  };
  auto present_ind = pick_swap_present_mode(swapchain_info.present_modes, preferred_modes);
  if (present_ind) {
    present_mode = swapchain_info.present_modes[present_ind.value];
  } else {
    return error_cast<Swapchain>(present_ind);
  }

  const auto extent = clamp_swap_extent(fb_dims.width, fb_dims.height, swapchain_info.capabilities);
  auto image_count = swapchain_info.capabilities.minImageCount + 1;
  if (swapchain_info.capabilities.maxImageCount > 0) {
    image_count = std::min(image_count, swapchain_info.capabilities.maxImageCount);
  }

  VkSwapchainCreateInfoKHR create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  create_info.surface = surface.handle;
  create_info.minImageCount = image_count;
  create_info.imageFormat = surface_format.format;
  create_info.imageColorSpace = surface_format.colorSpace;
  create_info.imageExtent = extent;
  create_info.imageArrayLayers = 1;
  create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  const uint32_t queue_family_indices[2] = {
    physical_device.queue_family_indices.graphics.value(),
    physical_device.queue_family_indices.present.value()
  };

  if (queue_family_indices[0] != queue_family_indices[1]) {
    GROVE_ASSERT(false);  //  @TODO
    create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    create_info.queueFamilyIndexCount = 2;
    create_info.pQueueFamilyIndices = queue_family_indices;
  } else {
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.queueFamilyIndexCount = 0;
    create_info.pQueueFamilyIndices = nullptr;
  }

  create_info.preTransform = swapchain_info.capabilities.currentTransform;
  create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  create_info.presentMode = present_mode;
  create_info.clipped = VK_TRUE;
  create_info.oldSwapchain = VK_NULL_HANDLE;

  return grove::create_swapchain(
    device.handle, &create_info, surface_format.format, extent, present_mode);
}

void vk::destroy_swapchain(vk::Swapchain* swapchain, VkDevice device) {
  if (device != VK_NULL_HANDLE) {
    for (auto& view : swapchain->image_views) {
      vk::destroy_image_view(&view, device);
    }
    vkDestroySwapchainKHR(device, swapchain->handle, GROVE_VK_ALLOC);
    swapchain->handle = VK_NULL_HANDLE;
    swapchain->images = {};
    swapchain->image_views = {};
    swapchain->extent = {};
    swapchain->image_format = VK_FORMAT_UNDEFINED;
  } else {
    assert(swapchain->images.empty() && swapchain->image_views.empty());
  }
}

const char* vk::to_string(VkPresentModeKHR present_mode) {
  switch (present_mode) {
    case VK_PRESENT_MODE_IMMEDIATE_KHR:
      return "VK_PRESENT_MODE_IMMEDIATE_KHR";
    case VK_PRESENT_MODE_MAILBOX_KHR:
      return "VK_PRESENT_MODE_MAILBOX_KHR";
    case VK_PRESENT_MODE_FIFO_KHR:
      return "VK_PRESENT_MODE_FIFO_KHR";
    case VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR:
      return "VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR";
    case VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR:
      return "VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR";
    default:
      return "<UNKNOWN>";
  }
}

GROVE_NAMESPACE_END
