#pragma once

#include <atomic>

namespace grove {

template <typename T>
struct Future {
public:
  template <typename... Args>
  bool is_ready(Args&&... args) const {
    return ready.load(args...);
  }

  template <typename... Args>
  void mark_ready(Args&&... args) {
    ready.store(true, args...);
  }

public:
  std::atomic<bool> ready{false};
  T data;
};

}