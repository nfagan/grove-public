#include "GLMouse.hpp"
#include "grove/common/common.hpp"
#include <GLFW/glfw3.h>

namespace {
  grove::GLMouse* active_mouse_instance = nullptr;
  std::mutex active_mouse_instance_mutex;
}

GROVE_NAMESPACE_BEGIN

void glfw::scroll_callback(GLFWwindow*, double x, double y) {
  std::lock_guard<std::mutex> lock(active_mouse_instance_mutex);

  if (active_mouse_instance == nullptr) {
    return;
  }

  active_mouse_instance->accumulate_scroll(float(x), float(y));
}

void glfw::cursor_position_callback(GLFWwindow*, double x, double y) {
  std::lock_guard<std::mutex> lock(active_mouse_instance_mutex);
  
  if (active_mouse_instance != nullptr) {
    active_mouse_instance->set_coordinates(x, y);
  }
}

void glfw::mouse_button_callback(GLFWwindow*, int button, int action, int) {
  std::lock_guard<std::mutex> lock(active_mouse_instance_mutex);

  if (active_mouse_instance == nullptr) {
    return;
  }

  auto grove_button = Mouse::Button::Left;
  bool recognized_button = true;

  switch (button) {
    case GLFW_MOUSE_BUTTON_LEFT:
      break;
    case GLFW_MOUSE_BUTTON_RIGHT:
      grove_button = Mouse::Button::Right;
      break;
    default:
      recognized_button = false;
  }

  if (!recognized_button) {
    return;
  }

  if (action == GLFW_PRESS) {
    active_mouse_instance->mark_pressed(grove_button);

  } else if (action == GLFW_RELEASE) {
    active_mouse_instance->mark_released(grove_button);
  }
}

GLMouse::GLMouse() :
  x(0.0),
  y(0.0),
  scroll_x(0.0f),
  scroll_y(0.0f),
  button_state{} {
  //
  make_active_instance();
}

GLMouse::~GLMouse() {
  std::lock_guard<std::mutex> lock(active_mouse_instance_mutex);
  
  if (this == active_mouse_instance) {
    active_mouse_instance = nullptr;
  }
}

void GLMouse::make_active_instance() {
  std::lock_guard<std::mutex> lock(active_mouse_instance_mutex);
  active_mouse_instance = this;
}

void GLMouse::set_coordinates(double to_x, double to_y) {
  x = to_x * scale_x + offset_x;
  y = to_y * scale_y + offset_y;
}

void GLMouse::set_frame(float sx, float sy, float ox, float oy) {
  scale_x = sx;
  scale_y = sy;
  offset_x = ox;
  offset_y = oy;
}

void GLMouse::set_frame(float sx, float sy) {
  set_frame(sx, sy, 0.0f, 0.0f);
}

Mouse::Coordinates GLMouse::get_coordinates() const {
  return std::make_pair(x.load(), y.load());
}

void GLMouse::accumulate_scroll(float x_, float y_) {
  scroll_x += x_;
  scroll_y += y_;
}

Mouse::Coordinates GLMouse::get_clear_scroll() {
  float res_x = scroll_x;
  float res_y = scroll_y;
  scroll_x = 0.0f;
  scroll_y = 0.0f;
  return {res_x, res_y};
}

void GLMouse::mark_pressed(Button button) {
  std::lock_guard<std::mutex> lock(pressed_mutex);
  button_state[int(button)] = true;
}

void GLMouse::mark_released(Button button) {
  std::lock_guard<std::mutex> lock(pressed_mutex);
  button_state[int(button)] = false;
}

bool GLMouse::is_pressed(Button button) const {
  std::lock_guard<std::mutex> lock(pressed_mutex);
  return button_state[int(button)];
}

GROVE_NAMESPACE_END

