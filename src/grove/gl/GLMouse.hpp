#pragma once

#include "grove/input/Mouse.hpp"
#include <atomic>
#include <mutex>
#include <array>

struct GLFWwindow;

namespace grove {

class GLMouse;

namespace glfw {
void cursor_position_callback(GLFWwindow* window, double x, double y);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void scroll_callback(GLFWwindow* window, double x, double y);
}

}

class grove::GLMouse : public grove::Mouse {
  friend void glfw::cursor_position_callback(GLFWwindow* window, double x, double y);
  friend void glfw::mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
  friend void glfw::scroll_callback(GLFWwindow* window, double x, double y);
public:
  GLMouse();
  ~GLMouse() override;
  
  Coordinates get_coordinates() const override;
  Coordinates get_clear_scroll();
  void set_coordinates(double to_x, double to_y) override;
  void set_frame(float sx, float sy, float ox, float oy) override;
  void set_frame(float sx, float sy);

  void mark_pressed(Button button) override;
  void mark_released(Button button) override;
  void accumulate_scroll(float x, float y);

  bool is_pressed(Button button) const override;

  void make_active_instance();
  
private:
  mutable std::mutex pressed_mutex;

  std::atomic<double> x;
  std::atomic<double> y;
  float scroll_x;
  float scroll_y;
  float scale_x{1.0f};
  float scale_y{1.0f};
  float offset_x{};
  float offset_y{};
  std::array<bool, Mouse::number_of_buttons()> button_state;
};
