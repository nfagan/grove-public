#include "device.hpp"
#include "physical_device.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

std::vector<VkDeviceQueueCreateInfo>
vk::make_device_queue_create_info_one_queue_per_family(const uint32_t* unique_family_indices,
                                                       uint32_t num_families,
                                                       const float* queue_priority) {
  std::vector<VkDeviceQueueCreateInfo> result;
  for (uint32_t i = 0; i < num_families; i++) {
    VkDeviceQueueCreateInfo queue_create_info{};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = unique_family_indices[i];
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = queue_priority;
    result.push_back(queue_create_info);
  }
  return result;
}

VkDeviceCreateInfo vk::make_device_create_info(const VkDeviceQueueCreateInfo* queue_create_info,
                                               uint32_t num_queues,
                                               const VkPhysicalDeviceFeatures* enable_features,
                                               const char* const* enable_exts,
                                               uint32_t num_exts,
                                               const char* const* enable_layers,
                                               uint32_t num_layers) {
  VkDeviceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  create_info.pQueueCreateInfos = queue_create_info;
  create_info.queueCreateInfoCount = num_queues;
  create_info.pEnabledFeatures = enable_features;
  create_info.enabledExtensionCount = num_exts;
  create_info.ppEnabledExtensionNames = enable_exts;

  if (num_layers > 0) {
    create_info.enabledLayerCount = num_layers;
    create_info.ppEnabledLayerNames = enable_layers;
  } else {
    create_info.enabledLayerCount = 0;
    create_info.ppEnabledLayerNames = nullptr;
  }

  return create_info;
}

const vk::DeviceQueue* vk::Device::ith_queue(uint32_t in_family, uint32_t index) const {
  if (auto it = queues.find(in_family); it != queues.end()) {
    if (index < uint32_t(it->second.size())) {
      return &it->second[index];
    }
  }
  return nullptr;
}

void vk::destroy_device(Device* device) {
  vkDestroyDevice(device->handle, GROVE_VK_ALLOC);
  device->handle = VK_NULL_HANDLE;
  device->queues = {};
  device->enabled_features = {};
}

vk::Result<vk::Device> vk::create_device(const PhysicalDevice& physical_device,
                                         const VkDeviceCreateInfo* create_info) {
  VkDevice device;
  auto create_res = vkCreateDevice(physical_device.handle, create_info, GROVE_VK_ALLOC, &device);
  if (create_res != VK_SUCCESS) {
    return {create_res, "Failed to create device."};
  }

  vk::Device result{};
  result.handle = device;
  result.enabled_features = *create_info->pEnabledFeatures;

  for (uint32_t i = 0; i < create_info->queueCreateInfoCount; i++) {
    const auto& queue_info = create_info->pQueueCreateInfos[i];
    const auto queue_fam = queue_info.queueFamilyIndex;
    uint32_t queue_count = queue_info.queueCount;
    std::vector<DeviceQueue> queues(queue_count);
    assert(result.queues.count(queue_fam) == 0);

    for (uint32_t j = 0; j < queue_info.queueCount; j++) {
      DeviceQueue queue{};
      queue.family = queue_fam;
      vkGetDeviceQueue(device, queue_fam, j, &queue.handle);
      queues[j] = queue;
    }

    result.queues[queue_fam] = std::move(queues);
  }

  return result;
}

GROVE_NAMESPACE_END
