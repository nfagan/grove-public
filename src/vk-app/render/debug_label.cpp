#include "debug_label.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include <cassert>

GROVE_NAMESPACE_BEGIN

namespace {

struct GlobalData {
  int stack{};
  VkInstance instance{};
  VkDevice device{};
  PFN_vkCmdBeginDebugUtilsLabelEXT label_begin{};
  PFN_vkCmdEndDebugUtilsLabelEXT label_end{};
  bool tried_acquire_begin{};
  bool tried_acquire_end{};
} global_data;

PFN_vkCmdBeginDebugUtilsLabelEXT require_label_begin() {
  if (global_data.label_begin) {
    return global_data.label_begin;
  }

  if (global_data.tried_acquire_begin) {
    return nullptr;
  } else {
    global_data.tried_acquire_begin = true;
  }

  if (global_data.instance == VK_NULL_HANDLE || global_data.device == VK_NULL_HANDLE) {
    GROVE_LOG_ERROR_CAPTURE_META("Vulkan instance or device is null.", "debug/label");
    return nullptr;
  }

  auto func = (PFN_vkCmdBeginDebugUtilsLabelEXT) vkGetDeviceProcAddr(
    global_data.device, "vkCmdBeginDebugUtilsLabelEXT");
  if (!func) {
    GROVE_LOG_ERROR_CAPTURE_META("Missing extension.", "debug/label");
    return nullptr;
  }

  global_data.label_begin = func;
  return func;
}

PFN_vkCmdEndDebugUtilsLabelEXT require_label_end() {
  if (global_data.label_end) {
    return global_data.label_end;
  }

  if (global_data.tried_acquire_end) {
    return nullptr;
  } else {
    global_data.tried_acquire_end = true;
  }

  if (global_data.instance == VK_NULL_HANDLE || global_data.device == VK_NULL_HANDLE) {
    GROVE_LOG_ERROR_CAPTURE_META("Vulkan instance or device is null.", "debug/label");
    return nullptr;
  }

  auto func = (PFN_vkCmdEndDebugUtilsLabelEXT) vkGetDeviceProcAddr(
    global_data.device, "vkCmdEndDebugUtilsLabelEXT");
  if (!func) {
    GROVE_LOG_ERROR_CAPTURE_META("Missing extension.", "debug/label");
    return nullptr;
  }

  global_data.label_end = func;
  return func;
}

} //

void vk::debug::initialize_debug_labels(VkInstance instance, VkDevice device) {
  global_data.instance = instance;
  global_data.device = device;
}

void vk::debug::terminate_debug_labels() {
  global_data = {};
}

void vk::debug::label_begin(VkCommandBuffer cmd, const char* label) {
  auto func = require_label_begin();
  if (!func) {
    return;
  }

  VkDebugUtilsLabelEXT info{
    VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
    nullptr,
    label,
    {}
  };

  func(cmd, &info);
  global_data.stack++;
}

void vk::debug::label_end(VkCommandBuffer cmd) {
  auto func = require_label_end();
  if (!func) {
    return;
  }

  func(cmd);
  global_data.stack--;
  assert(global_data.stack >= 0);
}

GROVE_NAMESPACE_END
