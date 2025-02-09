#include "glfw.hpp"
#include "grove/common/common.hpp"
#include "grove/common/scope.hpp"
#include <GLFW/glfw3.h>

GROVE_NAMESPACE_BEGIN

using namespace vk;

void GLFWContext::set_cursor_hidden(bool hidden) const {
  if (window) {
    auto cursor_mode = hidden ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_NORMAL;
    glfwSetInputMode(window, GLFW_CURSOR, cursor_mode);
  }
}

void GLFWContext::set_window_should_close(bool v) const {
  if (window) {
    glfwSetWindowShouldClose(window, v);
  }
}

void vk::destroy_and_terminate_glfw_context(GLFWContext* context) {
  if (context->window) {
    glfwDestroyWindow(context->window);
    context->window = nullptr;
  }
  if (context->initialized) {
    glfwTerminate();
    context->initialized = false;
  }
  context->framebuffer_width = 0;
  context->framebuffer_height = 0;
}

Result<GLFWContext> vk::create_and_initialize_glfw_context(const GLFWContextCreateInfo& info) {
  GLFWContext context{};
  bool success = false;
  GROVE_SCOPE_EXIT {
    if (!success) {
      destroy_and_terminate_glfw_context(&context);
    }
  };

  if (glfwInit()) {
    context.initialized = true;
  } else {
    return {VK_ERROR_UNKNOWN, "Failed to initialize GLFW."};
  }

  GLFWmonitor* fullscreen_monitor{};
  GLFWmonitor* primary_monitor = glfwGetPrimaryMonitor();
  if (info.fullscreen_window_index == GLFWContextCreateInfo::default_monitor_index) {
    fullscreen_monitor = primary_monitor;
  } else if (info.fullscreen_window_index >= 0) {
    int monitor_count;
    GLFWmonitor** monitors = glfwGetMonitors(&monitor_count);
    assert(info.fullscreen_window_index < monitor_count);
    fullscreen_monitor = monitors[info.fullscreen_window_index];
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  auto* window = glfwCreateWindow(
    info.window_width, info.window_height, info.window_title, nullptr, nullptr);
  if (!window) {
    return {VK_ERROR_UNKNOWN, "Failed to create GLFW window."};
  } else {
    context.window = window;
  }

  if (fullscreen_monitor) {
    const GLFWvidmode* mode = glfwGetVideoMode(fullscreen_monitor);
    glfwSetWindowMonitor(
      window, fullscreen_monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
  }

  if (primary_monitor) {
    glfwGetMonitorContentScale(
      primary_monitor, &context.monitor_content_scale_x, &context.monitor_content_scale_y);
  }

  glfwGetFramebufferSize(window, &context.framebuffer_width, &context.framebuffer_height);
  glfwSetWindowUserPointer(window, info.user_data);
  glfwSetFramebufferSizeCallback(window, info.framebuffer_resize_callback);
  glfwSetKeyCallback(window, info.key_callback);
  glfwSetCursorPosCallback(window, info.cursor_position_callback);
  glfwSetMouseButtonCallback(window, info.mouse_button_callback);
  glfwSetScrollCallback(window, info.scroll_callback);
  success = true;
  return context;
}

GROVE_NAMESPACE_END
