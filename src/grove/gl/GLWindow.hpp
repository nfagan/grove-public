#pragma once

#include "grove/visual/Window.hpp"

struct GLFWwindow;

namespace grove {
  class GLWindow;
}

class grove::GLWindow : public grove::Window {
public:
  GLWindow();
  explicit GLWindow(GLFWwindow* window);
  ~GLWindow() override;
  
  void swap_buffers() const override;
  void poll_events() const override;
  bool should_close() const override;
  grove::Window::Dimensions dimensions() const override;
  grove::Window::Dimensions framebuffer_dimensions() const override;
  void set_vsync(bool to) const override;
  void set_swap_interval(int interval) const override;
  
  void close_if_escape_pressed() override;
  void close() override;

  GLFWwindow* window_ptr() const;
  void set_cursor_hidden(bool state) const;

private:
  GLFWwindow* window;
  bool is_open;
};
