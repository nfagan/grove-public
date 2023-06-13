#include "instance.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Optional.hpp"
#include "grove/common/platform.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>

#define GROVE_REQUIRE_PORTABILITY_ENUMERATION_ON_MACOS (1)

GROVE_NAMESPACE_BEGIN

namespace {

using RequiredInstanceExtensions = std::vector<const char*>;
using RequiredLayers = std::vector<const char*>;

bool has_validation_layer(const std::vector<VkLayerProperties>& layer_props, const char* layer) {
  for (auto& props : layer_props) {
    if (std::strcmp(props.layerName, layer) == 0) {
      return true;
    }
  }
  return false;
}

bool has_validation_layers(const std::vector<VkLayerProperties>& layer_props,
                               const std::vector<const char*>& layers) {
  return std::all_of(layers.begin(), layers.end(), [&layer_props](auto& layer) {
    return has_validation_layer(layer_props, layer);
  });
}

std::vector<VkLayerProperties> enumerate_instance_layer_properties() {
  uint32_t count{};
  vkEnumerateInstanceLayerProperties(&count, nullptr);
  std::vector<VkLayerProperties> result(count);
  vkEnumerateInstanceLayerProperties(&count, result.data());
  return result;
}

RequiredLayers required_validation_layers(bool use_validation_layers,
                                          bool use_sync_layer) {
  RequiredLayers layers;
  if (use_validation_layers) {
    layers.push_back("VK_LAYER_KHRONOS_validation");
  }
  if (use_sync_layer) {
    layers.push_back("VK_LAYER_KHRONOS_synchronization2");
  }
  return layers;
}

RequiredInstanceExtensions required_instance_extensions(bool enable_debug_utils_ext,
                                                        bool enable_debug_report_ext,
                                                        const std::vector<const char*>& addtl) {
  std::vector<const char*> result;

  uint32_t num_glfw_extensions{};
  const char** glfw_extensions{};
  glfw_extensions = glfwGetRequiredInstanceExtensions(&num_glfw_extensions);

  for (uint32_t i = 0; i < num_glfw_extensions; i++) {
    result.push_back(glfw_extensions[i]);
  }

  if (enable_debug_utils_ext) {
    result.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }
  if (enable_debug_report_ext) {
    result.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
  }
#ifdef GROVE_MACOS
  result.push_back("VK_KHR_get_physical_device_properties2");
#if GROVE_REQUIRE_PORTABILITY_ENUMERATION_ON_MACOS
  result.push_back("VK_KHR_portability_enumeration");
#endif
#endif

  for (auto& add : addtl) {
    result.push_back(add);
  }

  return result;
}

vk::Result<VkInstance> create_instance(const VkInstanceCreateInfo* info) {
  VkInstance instance{};
  auto result = vkCreateInstance(info, GROVE_VK_ALLOC, &instance);
  if (result != VK_SUCCESS) {
    return {result, "Failed to create instance."};
  } else {
    return instance;
  }
}

VkApplicationInfo make_application_info() {
  VkApplicationInfo app_info{};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "My special app";
  app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.pEngineName = "none";
  app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.apiVersion = VK_API_VERSION_1_0;
  return app_info;
}

VkInstanceCreateInfo make_instance_create_info(VkApplicationInfo* app_info,
                                               const RequiredInstanceExtensions& enable_instance_extensions,
                                               const RequiredLayers& enable_layers,
                                               const Optional<VkDebugUtilsMessengerCreateInfoEXT>& debug_messenger_create_info) {
  VkInstanceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = app_info;
  create_info.enabledExtensionCount = uint32_t(enable_instance_extensions.size());
  create_info.ppEnabledExtensionNames = enable_instance_extensions.data();

  if (debug_messenger_create_info) {
    create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debug_messenger_create_info.value();
  }

#ifdef GROVE_MACOS
#if GROVE_REQUIRE_PORTABILITY_ENUMERATION_ON_MACOS
  create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif
#endif

  create_info.enabledLayerCount = uint32_t(enable_layers.size());
  create_info.ppEnabledLayerNames = enable_layers.data();
  return create_info;
}

VkDebugUtilsMessengerCreateInfoEXT
make_debug_utils_messenger_create_info(PFN_vkDebugUtilsMessengerCallbackEXT callback, void* user_data) {
  VkDebugUtilsMessengerCreateInfoEXT create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  create_info.pfnUserCallback = callback;
  create_info.pUserData = user_data;
  return create_info;
}

VkDebugReportCallbackCreateInfoEXT
make_debug_report_callback_create_info(vk::DebugReportCallback callback, void* user_data) {
  VkDebugReportCallbackCreateInfoEXT result{};
  result.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
  result.flags = VK_DEBUG_REPORT_DEBUG_BIT_EXT;
  result.pUserData = user_data;
  result.pfnCallback = callback;
  return result;
}

VkResult create_debug_utils_messenger_instance(VkInstance instance,
                                               const VkDebugUtilsMessengerCreateInfoEXT* create_info,
                                               VkDebugUtilsMessengerEXT* messenger) {
  auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
    instance, "vkCreateDebugUtilsMessengerEXT");
  if (func != nullptr) {
    return func(instance, create_info, GROVE_VK_ALLOC, messenger);
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}

void destroy_debug_utils_messenger_instance(VkInstance instance, VkDebugUtilsMessengerEXT messenger) {
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
    instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != nullptr) {
    func(instance, messenger, GROVE_VK_ALLOC);
  }
}

vk::Result<vk::DebugUtilsMessenger>
create_debug_utils_messenger(VkInstance instance,
                             const VkDebugUtilsMessengerCreateInfoEXT* create_info) {
  VkDebugUtilsMessengerEXT messenger;
  auto res = create_debug_utils_messenger_instance(instance, create_info, &messenger);
  if (res != VK_SUCCESS) {
    return {res, "Failed to make debug utils messenger."};
  } else {
    vk::DebugUtilsMessenger result{};
    result.handle = messenger;
    return result;
  }
}

void destroy_debug_utils_messenger(vk::DebugUtilsMessenger* messenger, VkInstance instance) {
  if (instance != VK_NULL_HANDLE) {
    destroy_debug_utils_messenger_instance(instance, messenger->handle);
    messenger->handle = VK_NULL_HANDLE;
  } else {
    GROVE_ASSERT(messenger->handle == VK_NULL_HANDLE);
  }
}

vk::Result<vk::DebugReportCallbackEXT>
create_debug_report_callback_ext(VkInstance instance,
                                 const VkDebugReportCallbackCreateInfoEXT* create_info) {
  vk::DebugReportCallbackEXT callback;
  auto func = (PFN_vkCreateDebugReportCallbackEXT) vkGetInstanceProcAddr(
    instance, "vkCreateDebugReportCallbackEXT");
  if (func == nullptr) {
    return {VK_ERROR_EXTENSION_NOT_PRESENT, "Failed to find debug report callback function."};
  }
  auto res = func(instance, create_info, GROVE_VK_ALLOC, &callback.handle);
  if (res != VK_SUCCESS) {
    return {res, "Failed to create debug report callback ext."};
  } else {
    return callback;
  }
}

void destroy_debug_report_callback_ext(vk::DebugReportCallbackEXT* callback, VkInstance instance) {
  if (instance != VK_NULL_HANDLE) {
    auto func = (PFN_vkDestroyDebugReportCallbackEXT) vkGetInstanceProcAddr(
      instance, "vkDestroyDebugReportCallbackEXT");
    if (func != nullptr) {
      func(instance, callback->handle, GROVE_VK_ALLOC);
    } else {
      GROVE_ASSERT(callback->handle == VK_NULL_HANDLE);
    }
  } else {
    GROVE_ASSERT(callback->handle == VK_NULL_HANDLE);
  }
}

} //  anon

vk::Result<vk::Instance> vk::create_instance(const InstanceCreateInfo& info) {
  auto layer_props = enumerate_instance_layer_properties();
  auto layers = required_validation_layers(
    info.validation_layers_enabled,
    info.sync_layers_enabled);
  if (!has_validation_layers(layer_props, layers)) {
    return {VK_ERROR_INITIALIZATION_FAILED, "Missing some required validation layers."};
  }

  Optional<VkDebugUtilsMessengerCreateInfoEXT> debug_messenger_create_info;
  if (info.debug_callback_enabled) {
    GROVE_ASSERT(info.debug_callback && info.debug_utils_enabled);
    debug_messenger_create_info = make_debug_utils_messenger_create_info(
      info.debug_callback, info.debug_callback_user_data);
  }

  Optional<VkDebugReportCallbackCreateInfoEXT> debug_report_callback_create_info;
  if (info.debug_report_callback_enabled) {
    GROVE_ASSERT(info.debug_report_callback && info.debug_utils_enabled);
    debug_report_callback_create_info = make_debug_report_callback_create_info(
      info.debug_report_callback, info.debug_callback_user_data);
  }

  auto app_info = make_application_info();
  auto instance_exts = required_instance_extensions(
    info.debug_utils_enabled,
    info.debug_report_callback_enabled,
    info.additional_required_extensions);
  auto create_info = make_instance_create_info(
    &app_info, instance_exts, layers, debug_messenger_create_info);

  Instance result{};
  auto inst_res = grove::create_instance(&create_info);
  if (!inst_res) {
    return error_cast<Instance>(inst_res);
  }

  result.handle = inst_res.value;
  result.enabled_layers = std::move(layers);

  if (debug_messenger_create_info) {
    auto debug_res = create_debug_utils_messenger(
      result.handle, &debug_messenger_create_info.value());
    if (debug_res) {
      result.debug_messenger = debug_res.value;
    } else {
      destroy_instance(&result);
      return error_cast<Instance>(debug_res);
    }
  }

  if (debug_report_callback_create_info) {
    auto report_res = create_debug_report_callback_ext(
      result.handle, &debug_report_callback_create_info.value());
    if (report_res) {
      result.debug_report_callback_ext = report_res.value;
    } else {
      destroy_instance(&result);
      return error_cast<Instance>(report_res);
    }
  }

  return result;
}

void vk::destroy_instance(vk::Instance* instance) {
  destroy_debug_utils_messenger(&instance->debug_messenger, instance->handle);
  instance->debug_messenger = {};

  destroy_debug_report_callback_ext(&instance->debug_report_callback_ext, instance->handle);
  instance->debug_report_callback_ext = {};

  vkDestroyInstance(instance->handle, GROVE_VK_ALLOC);
  instance->handle = VK_NULL_HANDLE;
}

GROVE_NAMESPACE_END
