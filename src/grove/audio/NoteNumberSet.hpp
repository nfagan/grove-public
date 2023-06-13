#pragma once

#include <bitset>

namespace grove {

struct NoteNumberSet {
  bool contains(uint8_t number) const {
    return contents[number];
  }
  void insert(uint8_t number) {
    contents[number] = true;
  }
  void erase(uint8_t number) {
    contents[number] = false;
  }

  std::bitset<256> contents{};
};

}