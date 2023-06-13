#pragma once

#include "../vk/vk.hpp"

struct GLFWwindow;

namespace grove::vk {

using GLFWKeyCallback = void(GLFWwindow*, int, int, int, int);
using GLFWCursorPositionCallback = void(GLFWwindow*, double, double);
using GLFWMouseButtonCallback = void(GLFWwindow*, int, int, int);
using GLFWFramebufferResizeCallback = void(GLFWwindow*, int, int);
using GLFWScrollCallback = void(GLFWwindow*, double, double);

struct GLFWContext {
  float window_aspect_ratio() const {
    return float(framebuffer_width) / float(framebuffer_height);
  }
  void set_cursor_hidden(bool hidden) const;
  void set_window_should_close(bool v) const;

  bool initialized{};
  GLFWwindow* window{};
  int framebuffer_width{};
  int framebuffer_height{};
};

struct GLFWContextCreateInfo {
  static constexpr int default_monitor_index = 1 << 16;

  const char* window_title{""};
  int window_width{1280};
  int window_height{720};
  void* user_data{};
  int fullscreen_window_index{-1};
  GLFWKeyCallback* key_callback{};
  GLFWCursorPositionCallback* cursor_position_callback{};
  GLFWMouseButtonCallback* mouse_button_callback{};
  GLFWFramebufferResizeCallback* framebuffer_resize_callback{};
  GLFWScrollCallback* scroll_callback{};
};

Result<GLFWContext> create_and_initialize_glfw_context(const GLFWContextCreateInfo& info);
void destroy_and_terminate_glfw_context(GLFWContext* context);
void glfw_set_cursor_hidden(GLFWContext* context, bool value);

}