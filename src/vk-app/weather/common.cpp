#include "common.hpp"
#include "grove/common/common.hpp"
#include <cassert>

GROVE_NAMESPACE_BEGIN

const char* weather::to_string(State state) {
  switch (state) {
    case State::Sunny:
      return "Sunny";
    case State::Overcast:
      return "Overcast";
    default:
      assert(false);
      return "";
  }
}

GROVE_NAMESPACE_END
