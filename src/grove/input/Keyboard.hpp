#pragma once

#include <array>

namespace grove {
  class Keyboard;
  
  enum class Key {
    W = 0,
    A,
    S,
    D,
    C,
    E,
    R,
    F,
    T,
    G,
    Y,
    H,
    U,
    J,
    I,
    K,
    O,
    L,
    P,
    Q,

    Z,
    X,
    V,
    B,
    N,
    M,
    Number0,
    Number1,
    Number2,
    Number3,
    Number4,
    Number5,
    Number6,
    Number7,
    Number8,
    Number9,
    LeftShift,
    LeftControl,
    Tab,
    Space,
    Enter,
    Backspace,
    Slash,
    Backslash,
    GraveAccent,
    Escape,

    Command,
    LeftAlt,
    RightAlt,

    LeftArrow,
    RightArrow,
    DownArrow,
    UpArrow,

    Equal,
    Minus,

    KEY_SIZE
  };
  
  enum class KeyState {
    Pressed,
    Released
  };

  using KeyArray = std::array<Key, int(Key::KEY_SIZE)>;
  KeyArray all_keys();
}

class grove::Keyboard {
public:
  virtual ~Keyboard() = default;
  
  virtual void set_key_state(Key key, KeyState state) = 0;
  virtual void mark_pressed(Key key) = 0;
  virtual void mark_released(Key key) = 0;
  
  virtual bool is_pressed(Key key) const = 0;
  
  static constexpr int number_of_keys() {
    return int(Key::KEY_SIZE);
  }
  
  static constexpr unsigned int key_index(Key key) {
    return static_cast<unsigned int>(key);
  }
};
