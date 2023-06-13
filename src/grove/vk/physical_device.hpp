#pragma once

#include "common.hpp"
#include "grove/common/Optional.hpp"
#include <vector>

namespace grove::vk {

struct SwapchainSupportInfo {
  VkSurfaceCapabilitiesKHR capabilities{};
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> present_modes;
};

struct PhysicalDeviceInfo {
  size_t min_uniform_buffer_offset_alignment() const {
    return properties.limits.minUniformBufferOffsetAlignment;
  }
  size_t min_storage_buffer_offset_alignment() const {
    return properties.limits.minStorageBufferOffsetAlignment;
  }

  VkPhysicalDeviceProperties properties{};
  VkPhysicalDeviceMemoryProperties memory_properties{};
  VkPhysicalDeviceFeatures features{};
  std::vector<VkExtensionProperties> supported_extensions;
  std::vector<VkQueueFamilyProperties> queue_families;
};

struct QueueFamilyIndices {
  bool rendering_supported() const {
    return graphics && present;
  }

  Optional<uint32_t> graphics;
  Optional<uint32_t> present;
  Optional<uint32_t> compute;
  Optional<uint32_t> transfer;
};

struct PhysicalDevice {
  std::vector<uint32_t> unique_queue_family_indices() const;
  bool rendering_supported() const {
    return queue_family_indices.rendering_supported();
  }
  Optional<VkSampleCountFlagBits> framebuffer_color_depth_sample_count_flag_bits(int num_samples) const;
  VkPhysicalDeviceDepthStencilResolveProperties get_depth_stencil_resolve_properties() const;

  VkPhysicalDevice handle{VK_NULL_HANDLE};
  PhysicalDeviceInfo info;
  QueueFamilyIndices queue_family_indices;
  std::vector<const char*> enabled_extensions;
};

PhysicalDevice make_physical_device(VkPhysicalDevice device,
                                    const PhysicalDeviceInfo& device_info,
                                    const QueueFamilyIndices& queue_family_indices,
                                    const std::vector<const char*>& enabled_exts);
void clear_physical_device(PhysicalDevice* device);

PhysicalDeviceInfo get_physical_device_info(VkPhysicalDevice device);
SwapchainSupportInfo get_swapchain_support_info(VkPhysicalDevice device, VkSurfaceKHR surface);
QueueFamilyIndices get_queue_family_indices(VkPhysicalDevice device,
                                            const std::vector<VkQueueFamilyProperties>& properties,
                                            VkSurfaceKHR surface);

Optional<int> find_rendering_device(const PhysicalDeviceInfo* info,
                                    const vk::SwapchainSupportInfo* swapchain_info,
                                    const vk::QueueFamilyIndices* queue_family_indices,
                                    int num_devices,
                                    const std::vector<const char*>& required_extensions);

bool format_has_features(VkPhysicalDevice physical_device,
                         VkFormat candidate,
                         VkImageTiling required_tiling,
                         VkFormatFeatureFlags required_features);

Result<VkFormat> select_format_with_features(VkPhysicalDevice physical_device,
                                             const VkFormat* candidates,
                                             uint32_t num_candidates,
                                             VkImageTiling required_tiling,
                                             VkFormatFeatureFlags required_features);

}