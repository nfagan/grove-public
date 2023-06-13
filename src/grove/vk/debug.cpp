#include "debug.hpp"
#include "common.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

[[maybe_unused]] constexpr const char* logging_id() {
  return "vk/debug";
}

VKAPI_ATTR VkBool32 VKAPI_CALL default_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                      VkDebugUtilsMessageTypeFlagsEXT type,
                                                      const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
                                                      void* user_data) {
  (void) severity;
  (void) type;
  (void) callback_data;
  (void) user_data;

  if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
      GROVE_LOG_WARNING_CAPTURE_META(callback_data->pMessage, logging_id());
    } else {
      GROVE_LOG_ERROR_CAPTURE_META(callback_data->pMessage, logging_id());
      GROVE_ASSERT(false);
    }
  }
  return VK_FALSE;
}

VKAPI_ATTR VkBool32 VKAPI_CALL default_debug_report_callback(VkDebugReportFlagsEXT flags,
                                                             VkDebugReportObjectTypeEXT object_type,
                                                             uint64_t object,
                                                             size_t location,
                                                             int32_t message_code,
                                                             const char* layer_prefix,
                                                             const char* message,
                                                             void* user_data) {
  (void) flags;
  (void) object_type;
  (void) object;
  (void) location;
  (void) message_code;
  (void) layer_prefix;
  (void) message;
  (void) user_data;

  if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
    GROVE_LOG_ERROR_CAPTURE_META(message, logging_id());
  } else if ((flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) ||
             (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)) {
    GROVE_LOG_WARNING_CAPTURE_META(message, logging_id());
  }

  return VK_FALSE;
}

vk::DebugCallback debug_callback{nullptr};
vk::DebugReportCallback debug_report_callback{nullptr};

} //  anon

void vk::initialize_default_debug_callbacks() {
  debug_callback = default_debug_callback;
  debug_report_callback = default_debug_report_callback;
}

vk::DebugReportCallback vk::get_debug_report_callback() {
  return debug_report_callback;
}

void vk::set_debug_report_callback(DebugReportCallback callback) {
  debug_report_callback = callback;
}

void vk::set_debug_callback(DebugCallback callback) {
  debug_callback = callback;
}

vk::DebugCallback vk::get_debug_callback() {
  return debug_callback;
}

GROVE_NAMESPACE_END
