#pragma once

#include "grove/math/constants.hpp"
#include <cmath>

namespace grove {

template <typename T>
T sum_complex_moduli(const T* data, int num_elements) {
  T mag{};
  for (int i = 0; i < num_elements; i++) {
    const T re = data[i * 2];
    const T im = data[i * 2 + 1];
    mag += std::sqrt(re * re + im * im);
  }
  return mag;
}

template <typename T>
void complex_moduli(const T* data, T* dst, int num_elements) {
  for (int i = 0; i < num_elements; i++) {
    const T re = data[i * 2];
    const T im = data[i * 2 + 1];
    dst[i] = std::sqrt(re * re + im * im);
  }
}

template <typename T>
void dft(const T* source, T* dest, int n) {
  double inv_n = 1.0 / double(n);

  for (int i = 0; i < n; i++) {
    int i0 = i * 2;
    int i1 = i * 2 + 1;

    dest[i0] = 0;
    dest[i1] = 0;

    for (int j = 0; j < n; j++) {
      const double w = grove::two_pi() * double(i) * double(j) * inv_n;
      const double re = source[j] * std::cos(w);
      const double im = -(source[j] * std::sin(w));

      dest[i0] += T(re);
      dest[i1] += T(im);
    }

    dest[i0] *= T(inv_n);
    dest[i1] *= T(inv_n);
  }
}

template <typename T>
void idft(const T* source, T* dest, int n) {
  double inv_n = 1.0 / double(n);

  for (int i = 0; i < n; i++) {
    dest[i] = 0;

    for (int j = 0; j < n; j++) {
      int j0 = j * 2;
      int j1 = j * 2 + 1;

      const double w = grove::two_pi() * double(i) * double(j) * inv_n;
      const double im = source[j0];
      const double re = source[j1];

      dest[i] += (re * std::cos(w) - im * std::sin(w));
    }
  }
}

}