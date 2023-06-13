#pragma once

#include "grove/math/util.hpp"
#include "grove/math/vector.hpp"
#include "grove/common/algorithm.hpp"
#include <algorithm>
#include <functional>
#include <thread>
#include <cassert>
#include <cmath>
#include <vector>

namespace grove::image {

template <typename T>
struct DefaultFloatConvert {};

template <>
struct DefaultFloatConvert<float> {
  static float from_float(float v) {
    return v;
  }
  static float to_float(float v) {
    return v;
  }
};

template <>
struct DefaultFloatConvert<uint8_t> {
  static uint8_t from_float(float v) {
    return uint8_t(clamp(std::rint(v), 0.0f, 255.0f));
  }
  static float to_float(uint8_t v) {
    return float(v);
  }
};

inline void clamped_window_index(int i, int m, int n, int n2, int* i0, int* i1) {
  *i0 = i - n2;
  *i1 = *i0 + n;
  *i0 = std::max(0, *i0);
  *i1 = std::min(m, *i1);
}

inline int ij_to_linear(int i, int j, int cols, int channels) {
  return (i * cols + j) * channels;
}

template <typename Descriptor>
int ij_to_linear(int i, int j, const Descriptor& desc) {
  return ij_to_linear(i, j, desc.cols(), desc.num_channels());
}

inline void ij_to_uv(int i, int j, int rows, int cols, float* u, float* v) {
  *u = (float(j) + 0.5f) / float(cols);
  *v = (float(i) + 0.5f) / float(rows);
}

inline Vec2f ij_to_uv(int i, int j, int rows, int cols) {
  Vec2f res;
  ij_to_uv(i, j, rows, cols, &res.x, &res.y);
  return res;
}

template <typename Descriptor>
Vec2f ij_to_uv(int i, int j, const Descriptor& desc) {
  return ij_to_uv(i, j, desc.rows(), desc.cols());
}

inline void uv_to_ij_unclamped(float u, float v, int rows, int cols, int* i, int* j) {
  u *= float(cols);
  v *= float(rows);
  *i = int(std::floor(v));
  *j = int(std::floor(u));
  assert(*i >= 0 && *j >= 0 && *i < rows && *j < cols);
}

inline void uv_to_ij_clamped(float u, float v, int rows, int cols, int* i, int* j) {
  u = clamp01_open(u);
  v = clamp01_open(v);
  uv_to_ij_unclamped(u, v, rows, cols, i, j);
}

template <typename Float>
void simple_box_filter(const Float* a, Float* out, Float* tmp, int r, int c, int nc, int k_size) {
  std::fill(out, out + r * c * nc, Float(0));
  std::fill(tmp, tmp + r * c * nc, Float(0));

  Float v = Float(1) / Float(k_size);
  auto k2 = k_size / 2;

  for (int i = 0; i < r; i++) {
    for (int j = 0; j < c; j++) {
      auto* dst = tmp + ij_to_linear(i, j, c, nc);
      for (int k = 0; k < k_size; k++) {
        auto col = k - k2 + j;
        if (col >= 0 && col < c) {
          auto* src = a + ij_to_linear(i, col, c, nc);
          for (int s = 0; s < nc; s++) {
            dst[s] += src[s] * v;
          }
        }
      }
    }
  }

  for (int i = 0; i < r; i++) {
    for (int j = 0; j < c; j++) {
      auto* dst = out + ij_to_linear(i, j, c, nc);
      for (int k = 0; k < k_size; k++) {
        auto row = k - k2 + i;
        if (row >= 0 && row < r) {
          auto* src = tmp + ij_to_linear(row, j, c, nc);
          for (int s = 0; s < nc; s++) {
            dst[s] += src[s] * v;
          }
        }
      }
    }
  }
}

template <typename Float, int Nc>
void simple_box_filter(const Float* a, Float* out, Float* tmp, int r, int c, int k_size) {
  std::fill(out, out + r * c * Nc, Float(0));
  std::fill(tmp, tmp + r * c * Nc, Float(0));

  Float v = Float(1) / Float(k_size);
  auto k2 = k_size / 2;

  for (int i = 0; i < r; i++) {
    for (int j = 0; j < c; j++) {
      auto* dst = tmp + ij_to_linear(i, j, c, Nc);
      for (int k = 0; k < k_size; k++) {
        auto col = k - k2 + j;
        if (col >= 0 && col < c) {
          auto* src = a + ij_to_linear(i, col, c, Nc);
          for (int s = 0; s < Nc; s++) {
            dst[s] += src[s] * v;
          }
        }
      }
    }
  }

  for (int i = 0; i < r; i++) {
    for (int j = 0; j < c; j++) {
      auto* dst = out + ij_to_linear(i, j, c, Nc);
      for (int k = 0; k < k_size; k++) {
        auto row = k - k2 + i;
        if (row >= 0 && row < r) {
          auto* src = tmp + ij_to_linear(row, j, c, Nc);
          for (int s = 0; s < Nc; s++) {
            dst[s] += src[s] * v;
          }
        }
      }
    }
  }
}

template <typename T>
struct DefaultAverage2 {};

template <>
struct DefaultAverage2<float> {
  static float evaluate(float a, float b) {
    if (b < a) {
      std::swap(a, b);
    }
    return (b - a) * 0.5f + a;
  }
};

template <>
struct DefaultAverage2<uint8_t> {
  static uint8_t evaluate(uint8_t a, uint8_t b) {
    if (b < a) {
      std::swap(a, b);
    }
    auto f = std::rint(float(b - a) * 0.5f + float(a));
    return uint8_t(clamp(f, 0.0f, 255.0f));
  }
};

template <typename T, typename Average2>
T median_sorted_range(T* tmp, int tmp_size) {
  const int mid = tmp_size / 2;
  if ((tmp_size % 2) == 1) {
    return T(tmp[mid]);
  } else {
    assert(mid > 0);
    return Average2::evaluate(tmp[mid], tmp[mid-1]);
  }
}

template <typename T, typename Average2>
T median_range(T* tmp, int tmp_size) {
#if 0
  if ((tmp_size % 2) == 1) {
    return *alg::quick_select_in_place(tmp, tmp + tmp_size, (tmp_size + 1) / 2);
  } else {
    auto v0 = *alg::quick_select_in_place(tmp, tmp + tmp_size, tmp_size / 2);
    auto v1 = *alg::quick_select_in_place(tmp, tmp + tmp_size, tmp_size / 2 + 1);
    return Average2::evaluate(v0, v1);
  }
#else
  std::sort(tmp, tmp + tmp_size);
  return median_sorted_range<T, Average2>(tmp, tmp_size);
#endif
}

template <typename T, int NC, int C, typename Average2 = DefaultAverage2<T>>
void median_filter(const T* src, int rows, int cols, int n, T* tmp, T* dst) {
  assert(n >= 1);
  const int n2 = n / 2;
  int li{};
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
      int i0;
      int i1;
      clamped_window_index(i, rows, n, n2, &i0, &i1);

      int j0;
      int j1;
      clamped_window_index(j, cols, n, n2, &j0, &j1);

      int wi{};
      for (int ii = i0; ii < i1; ii++) {
        for (int jj = j0; jj < j1; jj++) {
          const int srci = (cols * ii + jj) * NC;
          tmp[wi++] = src[srci + C];
        }
      }

      dst[li * NC + C] = median_range<T, Average2>(tmp, wi);
      li++;
    }
  }
}

template <typename T>
void median_filter_component_dispatch(const T* src, int rows, int cols, int nc, int n,
                                      T* tmp, T* dst) {
  switch (nc) {
    case 1: {
      median_filter<T, 1, 0>(src, rows, cols, n, tmp, dst);
      break;
    }
    case 2: {
      median_filter<T, 2, 0>(src, rows, cols, n, tmp, dst);
      median_filter<T, 2, 1>(src, rows, cols, n, tmp, dst);
      break;
    }
    case 3: {
      median_filter<T, 3, 0>(src, rows, cols, n, tmp, dst);
      median_filter<T, 3, 1>(src, rows, cols, n, tmp, dst);
      median_filter<T, 3, 2>(src, rows, cols, n, tmp, dst);
      break;
    }
    case 4: {
      median_filter<T, 4, 0>(src, rows, cols, n, tmp, dst);
      median_filter<T, 4, 1>(src, rows, cols, n, tmp, dst);
      median_filter<T, 4, 2>(src, rows, cols, n, tmp, dst);
      median_filter<T, 4, 3>(src, rows, cols, n, tmp, dst);
      break;
    }
    default: {
      assert(false && "Expected 1, 2, 3, or 4 components per pixel.");
    }
  }
}

template <typename T, int NC, int C, typename Average2 = DefaultAverage2<T>>
void median_filter_per_dimension(const T* src, int rows, int cols, int n, bool col_first, T* dst) {
  assert(n >= 1 && n <= 256);
  const int n2 = n / 2;
  int li{};
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
      int i0;
      int i1;
      clamped_window_index(i, rows, n, n2, &i0, &i1);

      int j0;
      int j1;
      clamped_window_index(j, cols, n, n2, &j0, &j1);

      T tmp0[256];
      T tmp1[256];

      int ri{};
      if (col_first) {
        for (int ii = i0; ii < i1; ii++) {
          int ci{};
          for (int jj = j0; jj < j1; jj++) {
            const int srci = (cols * ii + jj) * NC + C;
            tmp0[ci++] = src[srci];
          }
          tmp1[ri++] = median_range<T, Average2>(tmp0, ci);
        }
      } else {
        for (int jj = j0; jj < j1; jj++) {
          int ci{};
          for (int ii = i0; ii < i1; ii++) {
            const int srci = (cols * ii + jj) * NC + C;
            tmp0[ci++] = src[srci];
          }
          tmp1[ri++] = median_range<T, Average2>(tmp0, ci);
        }
      }

      dst[li * NC + C] = median_range<T, Average2>(tmp1, ri);
      li++;
    }
  }
}

template <typename T>
void median_filter_per_dimension_component_dispatch(const T* src, int rows, int cols, int nc,
                                                    int n, bool col_first, T* dst,
                                                    bool threaded = false) {
  switch (nc) {
    case 1: {
      median_filter_per_dimension<T, 1, 0>(src, rows, cols, n, col_first, dst);
      break;
    }
    case 2: {
      median_filter_per_dimension<T, 2, 0>(src, rows, cols, n, col_first, dst);
      median_filter_per_dimension<T, 2, 1>(src, rows, cols, n, col_first, dst);
      break;
    }
    case 3: {
      median_filter_per_dimension<T, 3, 0>(src, rows, cols, n, col_first, dst);
      median_filter_per_dimension<T, 3, 1>(src, rows, cols, n, col_first, dst);
      median_filter_per_dimension<T, 3, 2>(src, rows, cols, n, col_first, dst);
      break;
    }
    case 4: {
      if (threaded) {
        std::vector<std::function<void()>> fs;
        fs.reserve(4);
        fs.emplace_back([&]() { median_filter_per_dimension<T, 4, 0>(src, rows, cols, n, col_first, dst); });
        fs.emplace_back([&]() { median_filter_per_dimension<T, 4, 1>(src, rows, cols, n, col_first, dst); });
        fs.emplace_back([&]() { median_filter_per_dimension<T, 4, 2>(src, rows, cols, n, col_first, dst); });
        fs.emplace_back([&]() { median_filter_per_dimension<T, 4, 3>(src, rows, cols, n, col_first, dst); });
        std::vector<std::thread> threads;
        threads.reserve(4);
        for (auto& f : fs) {
          threads.emplace_back(std::move(f));
        }
        for (auto& th : threads) {
          th.join();
        }
      } else {
        median_filter_per_dimension<T, 4, 0>(src, rows, cols, n, col_first, dst);
        median_filter_per_dimension<T, 4, 1>(src, rows, cols, n, col_first, dst);
        median_filter_per_dimension<T, 4, 2>(src, rows, cols, n, col_first, dst);
        median_filter_per_dimension<T, 4, 3>(src, rows, cols, n, col_first, dst);
      }
      break;
    }
    default: {
      assert(false && "Expected 1, 2, 3, or 4 components per pixel.");
    }
  }
}

template <typename T>
void sample_nearest(const T* src, int rows, int cols, int channels, const Vec2f& uv, T* dst) {
  int i;
  int j;
  uv_to_ij_clamped(uv.x, uv.y, rows, cols, &i, &j);
  const auto ind = ij_to_linear(i, j, cols, channels);
  for (int c = 0; c < channels; c++) {
    dst[c] = src[ind + c];
  }
}

template <typename T, typename Descriptor>
void sample_nearest(const T* src, const Descriptor& desc, const Vec2f& uv, T* dst) {
  sample_nearest<T>(src, desc.rows(), desc.cols(), desc.num_channels(), uv, dst);
}

template <typename T, typename FloatConvert = DefaultFloatConvert<T>>
void sample_bilinear(const T* src, int rows, int cols, int channels, const Vec2f& uv, T* dst) {
  int i0;
  int j0;
  float u = clamp01_open(uv.x);
  float v = clamp01_open(uv.y);
  uv_to_ij_unclamped(u, v, rows, cols, &i0, &j0);

  const int i1 = std::min(i0 + 1, rows - 1);
  const int j1 = std::min(j0 + 1, cols - 1);

  const int ind00 = ij_to_linear(i0, j0, cols, channels);
  const int ind10 = ij_to_linear(i1, j0, cols, channels);
  const int ind01 = ij_to_linear(i0, j1, cols, channels);
  const int ind11 = ij_to_linear(i1, j1, cols, channels);

  for (int c = 0; c < channels; c++) {
    const float s00 = FloatConvert::to_float(src[ind00 + c]);
    const float s10 = FloatConvert::to_float(src[ind10 + c]);
    const float s01 = FloatConvert::to_float(src[ind01 + c]);
    const float s11 = FloatConvert::to_float(src[ind11 + c]);
    const float s = s00 + v * (s10 - s00) + u * (s01 - s00) + v * u * (s00 + s11 - s10 - s01);
    dst[c] = FloatConvert::from_float(s);
  }
}

template <typename T, typename Descriptor, typename FloatConvert = DefaultFloatConvert<T>>
void sample_bilinear(const T* src, const Descriptor& desc, const Vec2f& uv, T* dst) {
  sample_bilinear<T, FloatConvert>(src, desc.rows(), desc.cols(), desc.num_channels(), uv, dst);
}

inline double srgb_to_linear(double c) {
  if (c <= 0.0404482362771082) {
    return c / 12.92;
  } else {
    return std::pow((c + 0.055) / 1.055, 2.4);
  }
}

inline double linear_to_srgb(double c) {
  if (c <= 0.0031308) {
    return c * 12.92;
  } else {
    return 1.055 * pow(c, 1.0 / 2.4) - 0.055;
  }
}

inline Vec3f linear_to_srgb(const Vec3f& v) {
  return Vec3f{
    float(linear_to_srgb(v.x)),
    float(linear_to_srgb(v.y)),
    float(linear_to_srgb(v.z)),
  };
}

inline Vec3f srgb_to_linear(const Vec3f& v) {
  return Vec3f{
    float(srgb_to_linear(v.x)),
    float(srgb_to_linear(v.y)),
    float(srgb_to_linear(v.z)),
  };
}

//  size of dst is rows * cols * nc (i.e., size of src)
//  size of tmp is n * n
void median_filter_uint8n(const uint8_t* src, int rows, int cols, int nc, int n,
                          uint8_t* tmp, uint8_t* dst);

//  size of dst is rows * cols * nc (i.e., size of src), n is <= 256
void median_filter_per_dimension_uint8n(const uint8_t* src, int rows, int cols, int nc,
                                        int n, bool col_first, uint8_t* dst, bool threaded = false);

//  size of dst is rows * cols * nc (i.e., size of src), n is <= 256
void median_filter_per_dimension_floatn(const float* src, int rows, int cols, int nc,
                                        int n, bool col_first, float* dst, bool threaded = false);

void srgb_to_linear(const uint8_t* src, int rows, int cols, int channels, float* dst);

//  size of `src` is rows * cols; only single component images supported.
//  size of `h` is n * n; only square single channel kernels supported.
//  size of `dst` matches `src`
void xcorr(const float* src, int rows, int cols, const float* h, int n, bool norm_h, float* dst);

}