#pragma once

#include "constants.hpp"
#include <algorithm>
#include <cmath>

namespace grove {

template <typename T>
const T& clamp(const T& value, const T& lo, const T& hi) {
  if (value < lo) {
    return lo;
  } else if (value > hi) {
    return hi;
  } else {
    return value;
  }
}

template <typename T>
T clamp01(const T& value) {
  return clamp(value, T(0), T(1));
}

//  Clamp a floating point value in the half-open interval [0.0, 1.0)
template <typename Float>
Float clamp01_open(Float value) {
  return clamp(value, Float(0), std::nextafter(Float(1), -Float(1)));
}

//  Return a value constrained in magnitude to `max`, but with the same sign as `v`.
template <typename T>
T constrain_magnitude(const T& v, const T& max) {
  return std::copysign(std::min(std::abs(v), max), v);
}

template <typename T>
T radians(const T& value) {
  return (value * T(pi())) / T(180);
}

template <typename T>
T degrees(const T& value) {
  return (value * T(180)) / T(pi());
}

int next_pow2(int value);

template <typename T>
inline T min(const T& a, const T& b, const T& c) {
  return std::min(std::min(a, b), c);
}

template <typename T>
inline T max(const T& a, const T& b, const T& c) {
  return std::max(std::max(a, b), c);
}

template <typename T, typename U>
inline T lerp(U frac, const T& a, const T& b) {
  return (U(1) - frac) * a + frac * b;
}

template <typename T>
inline T inv_lerp_clamped(const T& v, const T& lo, const T& hi) {
  return lo == hi ? T(0) : (clamp(v, lo, hi) - lo) / (hi - lo);
}

template <typename U, typename T>
inline T lerp_exp(U tau, const T& a, const T& b) {
  auto t = std::exp(-tau);
  return t * a + (U(1) - t) * b;
}

template <typename Int, typename Float>
inline Int integer_lerp(Float frac, const Int& a, const Int& b) {
  auto dist = Int(Float(b - a) * frac);
  return a + dist;
}

template <typename Int, typename Float>
inline Int rounded_integer_lerp(Float frac, const Int& a, const Int& b) {
  auto res = Float(b - a) * frac + Float(a);
  return Int(std::round(res));
}

template <typename T>
void abs_max_normalize(T* begin, T* end) {
  T maximum = std::numeric_limits<T>::lowest();
  const auto span = end - begin;

  for (auto i = 0; i < span; i++) {
    T v = std::abs(*(begin + i));
    if (v > maximum) {
      maximum = v;
    }
  }

  if (maximum == T(0)) {
    return;
  }

  const T norm_factor = T(1) / maximum;

  for (auto i = 0; i < span; i++) {
    *(begin + i) *= norm_factor;
  }
}

}
