#pragma once

#include <cassert>
#include <cstdint>
#include <cmath>

namespace grove {

namespace detail {

template <typename T>
T integer_wrap(T value, T size) {
  assert(size > 0);
  if (value >= 0) {
    return value % size;
  } else {
    auto v = std::abs(value) % size;
#if 0
    return size - v;
#else
    return v == 0 ? 0 : size - v;
#endif
  }
}

template <typename T>
T float_wrap(T value, T size) {
  auto floor_cnt = std::floor(value / size);
  auto floor_v = floor_cnt * size;
  return value - floor_v;
}

template <typename T>
struct WrapImpl {};

template <>
struct WrapImpl<float> {
  static float evaluate(float value, float size) {
    return float_wrap(value, size);
  }
};

template <>
struct WrapImpl<double> {
  static double evaluate(double value, double size) {
    return float_wrap(value, size);
  }
};

template <>
struct WrapImpl<int> {
  static int evaluate(int value, int size) {
    return integer_wrap(value, size);
  }
};

template <>
struct WrapImpl<int64_t> {
  static int64_t evaluate(int64_t value, int64_t size) {
    return integer_wrap(value, size);
  }
};

template <typename T, typename U>
struct CountImpl {
  static void count_wraps(T* value, T size, U* count) {
    assert(size != 0);
    while (*value >= size) {
      (*value) -= size;
      (*count)++;
    }
    while (*value < 0) {
      (*value) += size;
      (*count)--;
    }
  }
};

template <typename T, typename U>
struct NonLoopedCount {
  static void count_wraps(T* value, T size, U* count) {
    assert(size != 0);
    auto v = *value;

    auto floor_cnt = std::floor(v / size);
    auto floor_v = floor_cnt * size;
    auto full_count = int64_t(floor_cnt);

    *value = v - floor_v;
    *count += full_count;
  }
};

template <typename T>
struct CountImpl<double, T> {
  static void count_wraps(double* value, double size, T* count) {
    return NonLoopedCount<double, T>::count_wraps(value, size, count);
  }
};

template <typename T>
struct CountImpl<float, T> {
  static void count_wraps(float* value, float size, T* count) {
    return NonLoopedCount<float, T>::count_wraps(value, size, count);
  }
};
} //  detail

template <typename T>
inline T wrap_within_range(T value, T size) {
  return detail::WrapImpl<T>::evaluate(value, size);
}

template <typename T, typename U>
inline void count_wraps_to_range(T* value, T size, U* count) {
  detail::CountImpl<T, U>::count_wraps(value, size, count);
}

}