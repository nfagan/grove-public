#pragma once

#include <cmath>

namespace grove::stats {

template <typename T>
T mean(const T* values, std::size_t size) {
  T sum{};
  for (std::size_t i = 0; i < size; i++) {
    sum += values[i];
  }
  return sum / T(size);
}

template <typename T>
double mean_double(const T* values, std::size_t size) {
  double sum{};
  for (std::size_t i = 0; i < size; i++) {
    sum += double(values[i]);
  }
  return sum / double(size);
}

template <typename T>
T mean_or_default(const T* values, std::size_t size, const T& v) {
  return size == 0 ? v : mean(values, size);
}

template <typename T>
T std_or_default(const T* values, std::size_t size, const T& v) {
  if (size == 0) {
    return v;

  } else {
    auto m = mean(values, size);
    T ss{};

    for (std::size_t i = 0; i < size; i++) {
      auto diff = values[i] - m;
      ss += diff * diff;
    }

    if (size > 1) {
      ss /= T(size - 1);
    }

    return std::sqrt(ss);
  }
}

}