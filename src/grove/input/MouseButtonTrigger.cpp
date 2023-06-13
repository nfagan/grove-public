#include "MouseButtonTrigger.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

/*
 * ButtonState
 */

MouseButtonTrigger::ButtonState::ButtonState() : is_present{} {
  //
}

void MouseButtonTrigger::ButtonState::clear() {
  std::fill(is_present.begin(), is_present.end(), false);
}

bool MouseButtonTrigger::ButtonState::contains(Mouse::Button button) const {
  return is_present[int(button)];
}

void MouseButtonTrigger::ButtonState::insert(Mouse::Button button) {
  is_present[int(button)] = true;
}

void MouseButtonTrigger::ButtonState::erase(Mouse::Button button) {
  is_present[int(button)] = false;
}

/*
 * MouseButtonTrigger
 */

MouseButtonTrigger::MouseButtonTrigger(Mouse* mouse) :
  mouse(mouse) {
  //
}

void MouseButtonTrigger::add_listener(Listener listener) {
  listeners.push_back(std::move(listener));
}

bool MouseButtonTrigger::newly_pressed(Mouse::Button button) const {
  return triggered_buttons.contains(button);
}

bool MouseButtonTrigger::newly_released(Mouse::Button button) const {
  return released_buttons.contains(button);
}

void MouseButtonTrigger::update() {
  triggered_buttons.clear();
  released_buttons.clear();

  bool any_state_changed = false;

  for (int i = 0; i < Mouse::number_of_buttons(); i++) {
    const auto button = Mouse::Button(i);
    const bool is_pressed = mouse->is_pressed(button);
    const bool is_active = active_buttons.contains(button);

    if (is_pressed && !is_active) {
      triggered_buttons.insert(button);
      active_buttons.insert(button);
      any_state_changed = true;

    } else if (!is_pressed && is_active) {
      active_buttons.erase(button);
      released_buttons.insert(button);
      any_state_changed = true;
    }
  }

  if (any_state_changed) {
    for (auto& listener : listeners) {
      listener(triggered_buttons, released_buttons);
    }
  }
}

GROVE_NAMESPACE_END
