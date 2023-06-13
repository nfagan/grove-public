#pragma once

#include "grove/input/Keyboard.hpp"
#include <array>
#include <functional>
#include <mutex>

struct GLFWwindow;

namespace grove {
  class GLKeyboard;
  
  namespace glfw {
    void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
  }
}

class grove::GLKeyboard : public grove::Keyboard {
  friend void glfw::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
  
private:
  static constexpr int num_keys = grove::Keyboard::number_of_keys();
  
public:
  GLKeyboard();
  ~GLKeyboard() override;
  
  void set_key_state(Key key, KeyState state) override;
  void mark_pressed(Key key) override;
  void mark_released(Key key) override;
  bool is_pressed(Key key) const override;
  
  void make_active_instance();
  
private:
  std::array<bool, num_keys> pressed_state;
  mutable std::mutex pressed_state_mutex;
  
  static void key_callback(GLKeyboard* keyboard, GLFWwindow* window,
                           int key, int scancode, int action, int mods);
};
