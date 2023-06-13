#include "Keyboard.hpp"

namespace grove {

KeyArray all_keys() {
  KeyArray result{};

  for (int i = 0; i < int(result.size()); i++) {
    result[i] = Key(i);
  }

  return result;
}

}