#pragma once

#include <vulkan/vulkan.h>

namespace grove::vk {

using DebugCallback = PFN_vkDebugUtilsMessengerCallbackEXT;
using DebugReportCallback = PFN_vkDebugReportCallbackEXT;

void initialize_default_debug_callbacks();

void set_debug_callback(DebugCallback callback);
DebugCallback get_debug_callback();

void set_debug_report_callback(DebugReportCallback callback);
DebugReportCallback get_debug_report_callback();

}