#pragma once

#include "common.hpp"
#include "debug.hpp"
#include <vector>

namespace grove::vk {

struct DebugUtilsMessenger {
  VkDebugUtilsMessengerEXT handle{VK_NULL_HANDLE};
};

struct DebugReportCallbackEXT {
  VkDebugReportCallbackEXT handle{VK_NULL_HANDLE};
};

struct InstanceCreateInfo {
  bool validation_layers_enabled{};
  bool sync_layers_enabled{};
  bool debug_utils_enabled{};
  bool debug_callback_enabled{};
  bool debug_report_callback_enabled{};
  DebugCallback debug_callback{nullptr};
  DebugReportCallback debug_report_callback{nullptr};
  void* debug_callback_user_data{nullptr};
  std::vector<const char*> additional_required_extensions;
};

struct Instance {
  VkInstance handle{VK_NULL_HANDLE};
  DebugUtilsMessenger debug_messenger{};
  DebugReportCallbackEXT debug_report_callback_ext{};
  std::vector<const char*> enabled_layers;
};

Result<Instance> create_instance(const InstanceCreateInfo& info);
void destroy_instance(Instance* instance);

}