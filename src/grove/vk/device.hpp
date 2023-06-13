#pragma once

#include "common.hpp"
#include <vector>
#include <unordered_map>

namespace grove::vk {

struct PhysicalDevice;

struct DeviceQueue {
  VkQueue handle{VK_NULL_HANDLE};
  uint32_t family{};
};

struct Device {
  const DeviceQueue* ith_queue(uint32_t in_family, uint32_t index) const;

  VkDevice handle{VK_NULL_HANDLE};
  std::unordered_map<uint32_t, std::vector<DeviceQueue>> queues;
  VkPhysicalDeviceFeatures enabled_features{};
};

std::vector<VkDeviceQueueCreateInfo>
make_device_queue_create_info_one_queue_per_family(const uint32_t* unique_family_indices,
                                                   uint32_t num_families,
                                                   const float* queue_priority);

VkDeviceCreateInfo make_device_create_info(const VkDeviceQueueCreateInfo* queue_create_info,
                                           uint32_t num_queues,
                                           const VkPhysicalDeviceFeatures* enable_features,
                                           const char* const* enable_exts,
                                           uint32_t num_exts,
                                           const char* const* enable_layers,
                                           uint32_t num_layers);

Result<Device> create_device(const PhysicalDevice& physical_device,
                             const VkDeviceCreateInfo* create_info);
void destroy_device(Device* device);

}