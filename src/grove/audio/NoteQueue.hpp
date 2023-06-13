#pragma once

#include "grove/common/DynamicArray.hpp"

namespace grove {

template <typename Note, int StackSize>
struct NoteQueue {
  bool empty() const {
    return size() == 0;
  }

  int size() const {
    return tail - head;
  }

  Note* begin() {
    return notes.data() + head;
  }

  Note* end() {
    return notes.data() + tail;
  }

  const Note* peek_front() const {
    if (size() > 0) {
      return &notes[head];
    } else {
      return nullptr;
    }
  }

  Note pop_front() {
    assert(head < tail);
    auto note = notes[head++];
    return note;
  }

  void push_back(const Note& note) {
    if (tail == int(notes.size())) {
      int64_t new_size = notes.empty() ? 8 : notes.size() * 2;
      notes.resize(new_size);
    }
    notes[tail++] = note;
  }

  void erase_to_head() {
    std::rotate(notes.data(), notes.data() + head, notes.data() + tail);
    auto new_size = tail - head;
    head = 0;
    tail = new_size;
  }

  bool required_allocation() const {
    return int(notes.size()) > StackSize;
  }

  DynamicArray<Note, StackSize> notes;
  int tail{};
  int head{};
};

}