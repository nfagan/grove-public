#pragma once

#include <limits>

namespace grove {
  template <typename T>
  struct Epsilon;

  template<>
  struct Epsilon<float> {
    static constexpr float value = 1e-7f;
  };

  template<>
  struct Epsilon<double> {
    static constexpr double value = 1e-10;
  };

  constexpr float feps() {
    return 0.000001f;
  }
  
  constexpr double deps() {
    return 0.000001;
  }
  
  constexpr double pi() {
    return 3.14159265358979323846264338327950288;
  }

  constexpr float pif() {
    return float(pi());
  }

  constexpr double two_pi() {
    return 6.28318530717958647692528676655900576;
  }

  constexpr double pi_over_two() {
    return pi() * 0.5;
  }

  constexpr double pi_over_four() {
    return pi() * 0.25;
  }

  constexpr double infinity() {
    return std::numeric_limits<double>::infinity();
  }

  constexpr float infinityf() {
    return std::numeric_limits<float>::infinity();
  }

  constexpr double golden_ratio() {
    return 1.61803398874989484820458683436563811;
  }
  
  double nan();
}
