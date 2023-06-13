#pragma once

namespace grove::weather {

enum class State {
  Sunny,
  Overcast
};

struct Status {
  State current{State::Sunny};
  State next{State::Overcast};
  float frac_next{};
  bool changed{};
};

const char* to_string(State state);

}