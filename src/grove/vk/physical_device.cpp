#include "physical_device.hpp"
#include "core.hpp"
#include "grove/common/common.hpp"
#include <algorithm>
#include <numeric>
#include <set>

GROVE_NAMESPACE_BEGIN

namespace {

std::vector<VkExtensionProperties> enumerate_device_extensions(VkPhysicalDevice device) {
  uint32_t count{};
  vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
  std::vector<VkExtensionProperties> result(count);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &count, result.data());
  return result;
}

std::vector<VkSurfaceFormatKHR>
enumerate_physical_device_surface_formats(VkPhysicalDevice device, VkSurfaceKHR surface) {
  uint32_t count{};
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, nullptr);
  std::vector<VkSurfaceFormatKHR> result(count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, result.data());
  return result;
}

std::vector<VkPresentModeKHR>
enumerate_physical_device_surface_present_modes(VkPhysicalDevice device,
                                                VkSurfaceKHR surface) {
  uint32_t count{};
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, nullptr);
  std::vector<VkPresentModeKHR> result(count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, result.data());
  return result;
}

std::vector<VkQueueFamilyProperties> enumerate_queue_family_properties(VkPhysicalDevice device) {
  uint32_t count{};
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
  std::vector<VkQueueFamilyProperties> result(count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, result.data());
  return result;
}

bool has_device_extensions(const std::vector<VkExtensionProperties>& exts,
                           const std::vector<const char*>& queries) {
  return std::all_of(queries.begin(), queries.end(), [&exts](const auto& query) {
    return std::any_of(exts.begin(), exts.end(), [&query](const auto& ext) {
      return std::strcmp(ext.extensionName, query) == 0;
    });
  });
}

bool is_device_suitable_for_rendering(const vk::PhysicalDeviceInfo& info,
                                      const vk::SwapchainSupportInfo& swapchain_info,
                                      const vk::QueueFamilyIndices& queue_family_indices,
                                      const std::vector<const char*>& required_extensions) {
  return queue_family_indices.graphics &&
         queue_family_indices.present &&
         has_device_extensions(info.supported_extensions, required_extensions) &&
         !swapchain_info.present_modes.empty() &&
         !swapchain_info.formats.empty();
}

uint32_t score_device_for_rendering(const vk::PhysicalDeviceInfo& info,
                                    const vk::SwapchainSupportInfo& swapchain_info,
                                    const vk::QueueFamilyIndices& queue_families,
                                    const std::vector<const char*>& required_exts) {
  if (!is_device_suitable_for_rendering(info, swapchain_info, queue_families, required_exts)) {
    return 0;
  }
  uint32_t score{};
  if (info.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
    score += 1000;
  }
  //  @FIXME: Basically arbitrary score criterion.
  score += info.properties.limits.maxImageDimension2D;
  return score;
}

template <typename T>
Optional<uint32_t> find_queue_family(T&& f, const VkQueueFamilyProperties* props, uint32_t count) {
  for (uint32_t i = 0; i < count; i++) {
    if (f(props[i], i)) {
      return Optional<uint32_t>(i);
    }
  }
  return NullOpt{};
}

Optional<VkSampleCountFlagBits>
sample_count_flag_bits_from_count(VkSampleCountFlags count_flags, int num_samples) {
  if (num_samples < 0 || num_samples > 64) {
    return NullOpt{};
  }

  const VkSampleCountFlagBits flag_bits[6] = {
    VK_SAMPLE_COUNT_2_BIT,
    VK_SAMPLE_COUNT_4_BIT,
    VK_SAMPLE_COUNT_8_BIT,
    VK_SAMPLE_COUNT_16_BIT,
    VK_SAMPLE_COUNT_32_BIT,
    VK_SAMPLE_COUNT_64_BIT,
  };

  for (uint32_t i = 1; i <= 6; i++) {
    uint32_t ct = 1u << i;
    if (ct == uint32_t(num_samples)) {
      if (count_flags & flag_bits[i-1]) {
        return Optional<VkSampleCountFlagBits>(flag_bits[i-1]);
      } else {
        return NullOpt{};
      }
    }
  }

  return NullOpt{};
}

} //  anon

vk::QueueFamilyIndices vk::get_queue_family_indices(VkPhysicalDevice device,
                                                    const std::vector<VkQueueFamilyProperties>& properties,
                                                    VkSurfaceKHR surface) {
  vk::QueueFamilyIndices result{};
  const auto* props = properties.data();
  const auto num_props = uint32_t(properties.size());

  //  graphics
  result.graphics = find_queue_family([](const VkQueueFamilyProperties& props, uint32_t) {
    return props.queueFlags & VK_QUEUE_GRAPHICS_BIT;
  }, props, num_props);

  //  present
  result.present = find_queue_family([device, surface](const auto&, uint32_t ind) {
    VkBool32 present_support{};
    vkGetPhysicalDeviceSurfaceSupportKHR(device, ind, surface, &present_support);
    return present_support;
  }, props, num_props);

  //  compute
  result.compute = find_queue_family([](const VkQueueFamilyProperties& props, uint32_t) {
    return props.queueFlags & VK_QUEUE_COMPUTE_BIT;
  }, props, num_props);

  //  transfer, look for dedicated
  auto transfer = find_queue_family([](const VkQueueFamilyProperties& props, uint32_t) {
    return (props.queueFlags & VK_QUEUE_TRANSFER_BIT) &&
           !(props.queueFlags & VK_QUEUE_GRAPHICS_BIT);
  }, props, num_props);

  if (transfer) {
    result.transfer = transfer.value();
  } else {
    result.transfer = result.graphics;
  }

  return result;
}

vk::SwapchainSupportInfo vk::get_swapchain_support_info(VkPhysicalDevice device,
                                                        VkSurfaceKHR surface) {
  vk::SwapchainSupportInfo result;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &result.capabilities);
  result.formats = enumerate_physical_device_surface_formats(device, surface);
  result.present_modes = enumerate_physical_device_surface_present_modes(device, surface);
  return result;
}


vk::PhysicalDeviceInfo vk::get_physical_device_info(VkPhysicalDevice device) {
  PhysicalDeviceInfo result{};
  vkGetPhysicalDeviceProperties(device, &result.properties);
  vkGetPhysicalDeviceMemoryProperties(device, &result.memory_properties);
  vkGetPhysicalDeviceFeatures(device, &result.features);
  result.supported_extensions = enumerate_device_extensions(device);
  result.queue_families = enumerate_queue_family_properties(device);
  return result;
}

Optional<int> vk::find_rendering_device(const PhysicalDeviceInfo* info,
                                        const vk::SwapchainSupportInfo* swapchain_info,
                                        const vk::QueueFamilyIndices* queue_family_indices,
                                        int num_devices,
                                        const std::vector<const char*>& required_extensions) {
  if (num_devices > 0) {
    std::vector<uint32_t> device_scores;
    std::vector<int> indices(num_devices);
    std::iota(indices.begin(), indices.end(), 0);

    for (int i = 0; i < num_devices; i++) {
      device_scores.push_back(score_device_for_rendering(
        info[i], swapchain_info[i], queue_family_indices[i], required_extensions));
    }

    std::sort(indices.begin(), indices.end(), [&device_scores](auto&& a, auto&& b) {
      return device_scores[a] < device_scores[b];
    });

    auto best_ind = indices.back();
    if (device_scores[best_ind] > 0) {
      return Optional<int>(best_ind);
    }
  }

  return NullOpt{};
}

vk::PhysicalDevice vk::make_physical_device(VkPhysicalDevice device,
                                            const PhysicalDeviceInfo& info,
                                            const QueueFamilyIndices& queue_family_indices,
                                            const std::vector<const char*>& enabled_exts) {
  PhysicalDevice result;
  result.handle = device;
  result.info = info;
  result.queue_family_indices = queue_family_indices;
  result.enabled_extensions = enabled_exts;
  return result;
}

void vk::clear_physical_device(PhysicalDevice* device) {
  device->handle = VK_NULL_HANDLE;
  device->info = {};
  device->queue_family_indices = {};
  device->enabled_extensions = {};
}

std::vector<uint32_t> vk::PhysicalDevice::unique_queue_family_indices() const {
  std::set<uint32_t> inds;
  if (queue_family_indices.graphics) {
    inds.insert(queue_family_indices.graphics.value());
  }
  if (queue_family_indices.present) {
    inds.insert(queue_family_indices.present.value());
  }
  if (queue_family_indices.transfer) {
    inds.insert(queue_family_indices.transfer.value());
  }
  if (queue_family_indices.compute) {
    inds.insert(queue_family_indices.compute.value());
  }
  std::vector<uint32_t> res{inds.begin(), inds.end()};
  return res;
}

Optional<VkSampleCountFlagBits>
vk::PhysicalDevice::framebuffer_color_depth_sample_count_flag_bits(int num_samples) const {
  const VkSampleCountFlags count_flags =
    info.properties.limits.framebufferColorSampleCounts &
    info.properties.limits.framebufferDepthSampleCounts;
  return grove::sample_count_flag_bits_from_count(count_flags, num_samples);
}

VkPhysicalDeviceDepthStencilResolveProperties
vk::PhysicalDevice::get_depth_stencil_resolve_properties() const {
  VkPhysicalDeviceDepthStencilResolveProperties depth_stencil_resolve_props{};
  depth_stencil_resolve_props.sType =
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES;

  VkPhysicalDeviceProperties2 physical_device_properties2;
  physical_device_properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  physical_device_properties2.pNext = &depth_stencil_resolve_props;

  vkGetPhysicalDeviceProperties2(handle, &physical_device_properties2);
  return depth_stencil_resolve_props;
}

bool vk::format_has_features(VkPhysicalDevice physical_device, VkFormat candidate,
                             VkImageTiling required_tiling,
                             VkFormatFeatureFlags required_features) {
  VkFormatProperties format_props;
  vkGetPhysicalDeviceFormatProperties(physical_device, candidate, &format_props);

  if (required_tiling == VK_IMAGE_TILING_LINEAR &&
      (format_props.linearTilingFeatures & required_features) == required_features) {
    return true;
  }
  if (required_tiling == VK_IMAGE_TILING_OPTIMAL &&
      (format_props.optimalTilingFeatures & required_features) == required_features) {
    return true;
  }

  return false;
}

vk::Result<VkFormat> vk::select_format_with_features(VkPhysicalDevice physical_device,
                                                     const VkFormat* candidates,
                                                     uint32_t num_candidates,
                                                     VkImageTiling required_tiling,
                                                     VkFormatFeatureFlags required_features) {
  for (uint32_t i = 0; i < num_candidates; i++) {
    if (format_has_features(physical_device, candidates[i], required_tiling, required_features)) {
      return candidates[i];
    }
  }
  return {VK_ERROR_FORMAT_NOT_SUPPORTED, "No format met requirements."};
}

GROVE_NAMESPACE_END
