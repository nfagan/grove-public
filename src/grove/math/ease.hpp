#pragma once

#include <cmath>

namespace grove::ease {

//  https://easings.net/

template <typename T>
T in_out_quad(const T& v) {
  return v < T(0.5) ? T(2) * v * v : T(1) - std::pow(T(-2) * v + T(2), T(2)) / T(2);
}

template <typename T>
T in_out_quart(const T& v) {
  return v < T(0.5) ? T(8) * v * v * v * v : T(1) - std::pow(T(-2) * v + T(2), T(4)) / T(2);
}

template <typename T>
T in_out_expo(const T& v) {
  return v == T(0)
    ? T(0)
    : v == T(1)
      ? T(1)
      : v < T(0.5) ? std::pow(T(2), T(20) * v - T(10)) / T(2)
      : (T(2) - std::pow(T(2), T(-20) * v + T(10))) / T(2);
}

template <typename T>
T expo_dt_aware(const T& v, const T& dt) {
  return T(1) - std::pow(T(1) - v, dt);
}

template <typename T>
T log(T t, T b) {
  assert(b != 0);
  if (b == T(1) || t == T(0) || t == T(1)) {
    return t;
  } else {
    return (std::pow(b, t) / b - T(1) / b) / (T(1) - T(1) / b);
  }
}

}