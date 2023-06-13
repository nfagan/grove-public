#pragma once

#include "instance.hpp"
#include "surface.hpp"
#include "physical_device.hpp"
#include "device.hpp"

struct GLFWwindow;

namespace grove::vk {

struct CoreCreateInfo {
  GLFWwindow* window{};
  InstanceCreateInfo instance_create_info{};
  std::vector<const char*> additional_required_physical_device_extensions{};
};

struct Core {
  const DeviceQueue* ith_graphics_queue(uint32_t i) const;
  const DeviceQueue* ith_present_queue(uint32_t i) const;
  bool ith_graphics_queue_and_family(const DeviceQueue** queue,
                                     uint32_t* queue_family,
                                     uint32_t i) const;

  Instance instance;
  Surface surface;
  PhysicalDevice physical_device;
  Device device;
};

Result<Core> create_core(const CoreCreateInfo& info);
void destroy_core(Core* core);

}