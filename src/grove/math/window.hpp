#pragma once

#include "constants.hpp"
#include <cmath>

namespace grove::win {

/*
 * [1] fredric j. harris [sic], On the Use of Windows for Harmonic Analysis with the
 *     Discrete Fourier Transform, Proceedings of the IEEE, Vol. 66, No. 1, January 1978
 */

template <typename Float>
void gauss1d(Float* dst, int n, Float alpha = Float(2.5)) {
  constexpr Float point5 = Float(0.5);
  const Float l2 = Float(n-1) * point5;
  for (int i = 0; i < n; i++) {
    const Float ni = Float(i) - l2;
    const Float t = alpha * ni / l2;
    dst[i] = std::exp(-point5 * (t * t));
  }
}

template <typename Float>
void gauss2d(Float* dst, int n, Float sigma, bool norm) {
  const Float s2 = sigma * sigma;
  const Float sig_factor = Float(1) / (Float(2) * Float(pi()) * s2);
  const int n2 = n / 2;

  Float s{};
  int li{};

  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      const auto i0 = Float(i - n2);
      const auto j0 = Float(j - n2);
      const auto v = sig_factor * std::exp(-(i0*i0 + j0*j0) / (Float(2) * s2));
      dst[li++] = v;
      s += v;
    }
  }

  if (norm) {
    for (int i = 0; i < n * n; i++) {
      dst[i] /= s;
    }
  }
}

}