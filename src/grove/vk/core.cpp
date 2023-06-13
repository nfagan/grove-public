#include "core.hpp"
#include "grove/common/common.hpp"
#include "grove/common/scope.hpp"
#include "grove/common/platform.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

std::vector<VkPhysicalDevice> enumerate_physical_devices(VkInstance instance) {
  uint32_t count{};
  vkEnumeratePhysicalDevices(instance, &count, nullptr);
  std::vector<VkPhysicalDevice> result(count);
  vkEnumeratePhysicalDevices(instance, &count, result.data());
  return result;
}

std::vector<const char*> required_device_extensions() {
  std::vector<const char*> exts{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#ifdef GROVE_MACOS
  exts.push_back("VK_KHR_portability_subset");
#endif
  return exts;
}

vk::Result<int> pick_physical_device(const std::vector<vk::PhysicalDeviceInfo>& physical_devices,
                                     const std::vector<vk::SwapchainSupportInfo>& swapchain_info,
                                     const std::vector<vk::QueueFamilyIndices>& queue_families,
                                     const std::vector<const char*>& required_extensions) {
  auto find_res = find_rendering_device(
    physical_devices.data(),
    swapchain_info.data(),
    queue_families.data(),
    int(queue_families.size()),
    required_extensions);
  if (find_res) {
    return find_res.value();
  } else {
    return {VK_ERROR_INITIALIZATION_FAILED, "Failed to find suitable rendering device."};
  }
}

vk::Result<vk::PhysicalDevice> create_physical_device(const vk::Instance& instance,
                                                      const vk::Surface& surface,
                                                      const vk::CoreCreateInfo& core_create_info) {
  auto devices = enumerate_physical_devices(instance.handle);

  std::vector<vk::PhysicalDeviceInfo> info;
  std::vector<vk::QueueFamilyIndices> queue_families;
  std::vector<vk::SwapchainSupportInfo> swapchain_support_info;

  for (auto& dev : devices) {
    info.push_back(vk::get_physical_device_info(dev));
    swapchain_support_info.push_back(vk::get_swapchain_support_info(dev, surface.handle));
    queue_families.push_back(vk::get_queue_family_indices(
      dev, info.back().queue_families, surface.handle));
  }

  auto exts = required_device_extensions();
  for (auto& ext : core_create_info.additional_required_physical_device_extensions) {
    exts.push_back(ext);
  }

  if (auto res = pick_physical_device(info, swapchain_support_info, queue_families, exts)) {
    const int ind = res.value;
    return vk::make_physical_device(devices[ind], info[ind], queue_families[ind], exts);
  } else {
    return vk::error_cast<vk::PhysicalDevice>(res);
  }
}

vk::Result<vk::Device> create_device(const vk::Instance& instance,
                                     const vk::PhysicalDevice& physical_device) {
  //  @TODO: Specify features.
  VkPhysicalDeviceFeatures enable_features{};

  const char* const* layers = instance.enabled_layers.data();
  const auto num_layers = uint32_t(instance.enabled_layers.size());

  const auto* extensions = physical_device.enabled_extensions.data();
  const auto num_extensions = uint32_t(physical_device.enabled_extensions.size());

  auto unique_queue_families = physical_device.unique_queue_family_indices();
  const float queue_priority = 1.0f;
  const auto* queue_families = unique_queue_families.data();
  const auto num_queue_families = uint32_t(unique_queue_families.size());

  auto queue_create_infos = vk::make_device_queue_create_info_one_queue_per_family(
    queue_families, num_queue_families, &queue_priority);

  auto device_create_info = vk::make_device_create_info(
    queue_create_infos.data(), uint32_t(queue_create_infos.size()),
    &enable_features, extensions, num_extensions, layers, num_layers);

  return vk::create_device(physical_device, &device_create_info);
}

} //  anon

vk::Result<vk::Core> vk::create_core(const CoreCreateInfo& info) {
  vk::Core core;
  bool success = false;
  GROVE_SCOPE_EXIT {
    if (!success) {
      destroy_core(&core);
    }
  };

  if (auto instance = create_instance(info.instance_create_info)) {
    core.instance = std::move(instance.value);
  } else {
    return error_cast<Core>(instance);
  }

  if (auto surface = create_surface(core.instance.handle, info.window)) {
    core.surface = std::move(surface.value);
  } else {
    return error_cast<Core>(surface);
  }

  if (auto device = create_physical_device(core.instance, core.surface, info)) {
    core.physical_device = std::move(device.value);
  } else {
    return error_cast<Core>(device);
  }

  if (auto device = grove::create_device(core.instance, core.physical_device)) {
    core.device = std::move(device.value);
  } else {
    return error_cast<Core>(device);
  }

  success = true;
  return core;
}

void vk::destroy_core(Core* core) {
  if (core->device.handle != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(core->device.handle);
  }

  destroy_device(&core->device);
  clear_physical_device(&core->physical_device);
  destroy_surface(&core->surface, core->instance.handle);
  destroy_instance(&core->instance);
}

const vk::DeviceQueue* vk::Core::ith_graphics_queue(uint32_t i) const {
  if (physical_device.queue_family_indices.graphics) {
    return device.ith_queue(physical_device.queue_family_indices.graphics.value(), i);
  } else {
    return nullptr;
  }
}

const vk::DeviceQueue* vk::Core::ith_present_queue(uint32_t i) const {
  if (physical_device.queue_family_indices.present) {
    return device.ith_queue(physical_device.queue_family_indices.present.value(), i);
  } else {
    return nullptr;
  }
}

bool vk::Core::ith_graphics_queue_and_family(const DeviceQueue** queue,
                                             uint32_t* queue_family,
                                             uint32_t i) const {
  if (physical_device.queue_family_indices.graphics) {
    uint32_t queue_fam = physical_device.queue_family_indices.graphics.value();
    auto* maybe_queue = device.ith_queue(queue_fam, i);
    if (maybe_queue) {
      *queue = maybe_queue;
      *queue_family = queue_fam;
      return true;
    }
  }
  return false;
}

GROVE_NAMESPACE_END
