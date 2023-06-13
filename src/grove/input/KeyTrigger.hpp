#pragma once

#include "Keyboard.hpp"
#include <functional>
#include <vector>
#include <unordered_set>

namespace grove {

class KeyTrigger {
public:
  using KeyState = std::unordered_set<Key>;
  using Listener = std::function<void(const KeyState&, const KeyState&)>;
  using Listeners = std::vector<Listener>;

public:
  explicit KeyTrigger(Keyboard* keyboard);

  void add_listener(Listener listener);
  void update();
  bool is_pressed(Key key) const;
  bool newly_pressed(Key key) const;
  bool newly_released(Key key) const;
  const KeyState& read_newly_pressed() const {
    return triggered_keys;
  }
  const KeyState& read_newly_released() const {
    return released_keys;
  }

  size_t num_listeners() const {
    return listeners.size();
  }

private:
  Keyboard* keyboard;
  Listeners listeners;
  KeyState active_keys;
  KeyState triggered_keys;
  KeyState released_keys;
};

}