#include "GLKeyboard.hpp"
#include "grove/common/common.hpp"
#include <GLFW/glfw3.h>
#include <unordered_map>

GROVE_NAMESPACE_BEGIN

namespace {
  GLKeyboard* active_keyboard_instance = nullptr;
  std::mutex active_keyboard_instance_mutex;
  
  KeyState from_glfw_key_action(int action) {
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
      return KeyState::Pressed;
    } else {
      return KeyState::Released;
    }
  }

  Key* from_glfw_key(int key) {
    static std::unordered_map<int, Key> key_map{
      {GLFW_KEY_W, Key::W},
      {GLFW_KEY_A, Key::A},
      {GLFW_KEY_S, Key::S},
      {GLFW_KEY_D, Key::D},
      {GLFW_KEY_C, Key::C},
      {GLFW_KEY_E, Key::E},
      {GLFW_KEY_R, Key::R},
      {GLFW_KEY_F, Key::F},
      {GLFW_KEY_T, Key::T},
      {GLFW_KEY_G, Key::G},
      {GLFW_KEY_Y, Key::Y},
      {GLFW_KEY_H, Key::H},
      {GLFW_KEY_U, Key::U},
      {GLFW_KEY_J, Key::J},
      {GLFW_KEY_I, Key::I},
      {GLFW_KEY_K, Key::K},
      {GLFW_KEY_O, Key::O},
      {GLFW_KEY_L, Key::L},
      {GLFW_KEY_P, Key::P},
      {GLFW_KEY_Q, Key::Q},
      {GLFW_KEY_Z, Key::Z},
      {GLFW_KEY_X, Key::X},
      {GLFW_KEY_V, Key::V},
      {GLFW_KEY_B, Key::B},
      {GLFW_KEY_N, Key::N},
      {GLFW_KEY_M, Key::M},
      {GLFW_KEY_0, Key::Number0},
      {GLFW_KEY_1, Key::Number1},
      {GLFW_KEY_2, Key::Number2},
      {GLFW_KEY_3, Key::Number3},
      {GLFW_KEY_4, Key::Number4},
      {GLFW_KEY_5, Key::Number5},
      {GLFW_KEY_6, Key::Number6},
      {GLFW_KEY_7, Key::Number7},
      {GLFW_KEY_8, Key::Number8},
      {GLFW_KEY_9, Key::Number9},
      {GLFW_KEY_LEFT_SHIFT, Key::LeftShift},
      {GLFW_KEY_LEFT_CONTROL, Key::LeftControl},
      {GLFW_KEY_TAB, Key::Tab},
      {GLFW_KEY_ENTER, Key::Enter},
      {GLFW_KEY_BACKSPACE, Key::Backspace},
      {GLFW_KEY_GRAVE_ACCENT, Key::GraveAccent},
      {GLFW_KEY_SLASH, Key::Slash},
      {GLFW_KEY_BACKSLASH, Key::Backslash},
      {GLFW_KEY_LEFT_SUPER, Key::Command},
      {GLFW_KEY_LEFT_ALT, Key::LeftAlt},
      {GLFW_KEY_RIGHT_ALT, Key::RightAlt},
      {GLFW_KEY_SPACE, Key::Space},
      {GLFW_KEY_LEFT, Key::LeftArrow},
      {GLFW_KEY_RIGHT, Key::RightArrow},
      {GLFW_KEY_DOWN, Key::DownArrow},
      {GLFW_KEY_UP, Key::UpArrow},
      {GLFW_KEY_EQUAL, Key::Equal},
      {GLFW_KEY_MINUS, Key::Minus},
      {GLFW_KEY_ESCAPE, Key::Escape}
    };

    auto it = key_map.find(key);
    if (it != key_map.end()) {
      return &it->second;
    } else {
      return nullptr;
    }
  }
}

void glfw::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  std::lock_guard<std::mutex> lock(active_keyboard_instance_mutex);
  
  if (active_keyboard_instance != nullptr) {
    GLKeyboard::key_callback(active_keyboard_instance, window, key, scancode, action, mods);
  }
}

void GLKeyboard::key_callback(grove::GLKeyboard* keyboard, GLFWwindow*, int key, int, int action, int) {
  const auto pressed_state = from_glfw_key_action(action);
  const auto* maybe_key = from_glfw_key(key);

  if (maybe_key) {
    keyboard->set_key_state(*maybe_key, pressed_state);
  }
}

GLKeyboard::GLKeyboard() : pressed_state{} {
  make_active_instance();
}

GLKeyboard::~GLKeyboard() {
  std::unique_lock<std::mutex> lock(active_keyboard_instance_mutex);
  
  if (active_keyboard_instance == this) {
    active_keyboard_instance = nullptr;
  }
}

void GLKeyboard::make_active_instance() {
  std::unique_lock<std::mutex> lock(active_keyboard_instance_mutex);
  active_keyboard_instance = this;
}

void GLKeyboard::set_key_state(grove::Key key, grove::KeyState state) {
  if (state == KeyState::Pressed) {
    mark_pressed(key);
  } else {
    mark_released(key);
  }
}

void GLKeyboard::mark_pressed(Key key) {
  std::lock_guard<std::mutex> lock(pressed_state_mutex);
  pressed_state[Keyboard::key_index(key)] = true;
}

void GLKeyboard::mark_released(Key key) {
  std::lock_guard<std::mutex> lock(pressed_state_mutex);
  pressed_state[Keyboard::key_index(key)] = false;
}

bool GLKeyboard::is_pressed(Key key) const {
  std::lock_guard<std::mutex> lock(pressed_state_mutex);
  return pressed_state[Keyboard::key_index(key)];
}

GROVE_NAMESPACE_END
