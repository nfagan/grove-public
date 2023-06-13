#include "KeyTrigger.hpp"

namespace grove {

KeyTrigger::KeyTrigger(Keyboard* keyboard) :
  keyboard(keyboard) {
  //
}

bool KeyTrigger::is_pressed(Key key) const {
  return keyboard->is_pressed(key);
}

bool KeyTrigger::newly_pressed(Key key) const {
  return triggered_keys.count(key);
}

bool KeyTrigger::newly_released(Key key) const {
  return released_keys.count(key);
}

void KeyTrigger::update() {
  triggered_keys.clear();
  released_keys.clear();

  for (int i = 0; i < Keyboard::number_of_keys(); i++) {
    const auto key = Key(i);
    const bool is_pressed = keyboard->is_pressed(key);
    const bool is_active = active_keys.count(key) > 0;

    if (is_pressed && !is_active) {
      triggered_keys.insert(key);
      active_keys.insert(key);

    } else if (!is_pressed && is_active) {
      active_keys.erase(key);
      released_keys.insert(key);
    }
  }

  if (!triggered_keys.empty() || !released_keys.empty()) {
    for (auto& listener : listeners) {
      listener(triggered_keys, released_keys);
    }
  }
}

void KeyTrigger::add_listener(Listener listener) {
  listeners.push_back(std::move(listener));
}

}
