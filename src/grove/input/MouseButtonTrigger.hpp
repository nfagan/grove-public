#pragma once

#include "Mouse.hpp"
#include <unordered_set>
#include <functional>
#include <vector>
#include <array>

namespace grove {

class MouseButtonTrigger {
public:
  class ButtonState {
  public:
    ButtonState();

    bool contains(Mouse::Button button) const;
    void insert(Mouse::Button button);
    void erase(Mouse::Button button);
    void clear();

  private:
    std::array<bool, Mouse::number_of_buttons()> is_present;
  };

  using Listener = std::function<void(const ButtonState&, const ButtonState&)>;
  using Listeners = std::vector<Listener>;

public:
  explicit MouseButtonTrigger(Mouse* mouse);

  void add_listener(Listener listener);
  bool newly_pressed(Mouse::Button button) const;
  bool newly_released(Mouse::Button button) const;
  void update();
  size_t num_listeners() const {
    return listeners.size();
  }
  Mouse::Coordinates get_coordinates() const {
    return mouse->get_coordinates();
  }

private:
  Mouse* mouse;
  Listeners listeners;
  ButtonState active_buttons;
  ButtonState triggered_buttons;
  ButtonState released_buttons;
};

}