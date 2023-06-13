#include "image_process.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

void image::median_filter_uint8n(const uint8_t* src, int rows, int cols, int nc, int n,
                                 uint8_t* tmp, uint8_t* dst) {
  median_filter_component_dispatch<uint8_t>(src, rows, cols, nc, n, tmp, dst);
}

void image::median_filter_per_dimension_uint8n(const uint8_t* src, int rows, int cols, int nc,
                                               int n, bool col_first, uint8_t* dst, bool threaded) {
  median_filter_per_dimension_component_dispatch<uint8_t>(
    src, rows, cols, nc, n, col_first, dst, threaded);
}

void image::median_filter_per_dimension_floatn(const float* src, int rows, int cols, int nc,
                                               int n, bool col_first, float* dst, bool threaded) {
  median_filter_per_dimension_component_dispatch<float>(
    src, rows, cols, nc, n, col_first, dst, threaded);
}

void image::srgb_to_linear(const uint8_t* src, int rows, int cols, int channels, float* dst) {
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
      auto ind = image::ij_to_linear(i, j, cols, channels);
      for (int c = 0; c < channels; c++) {
        dst[c] = float(clamp01(srgb_to_linear(float(src[ind + c]) / 255.0f)));
      }
    }
  }
}

void image::xcorr(const float* src, int rows, int cols, const float* h, int n, bool norm_h,
                  float* dst) {
  const int n2 = n / 2;

  int li{};
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
      int i0 = i - n2;
      int i1 = i0 + n;
      int j0 = j - n2;
      int j1 = j0 + n;

      const int hi0 = std::abs(std::min(0, i0));
      const int hi1 = n - std::max(0, i1 - rows);
      const int hj0 = std::abs(std::min(0, j0));
      const int hj1 = n - std::max(0, j1 - cols);

      i0 = std::max(0, i0);
      i1 = std::min(rows, i1);
      j0 = std::max(0, j0);
      j1 = std::min(cols, j1);

      const int is = i1 - i0;
      const int js = j1 - j0;
      assert(is == hi1 - hi0 && js == hj1 - hj0);
      assert(is <= n && js <= n);
      (void) hi1;
      (void) hj1;

      float s{};
      float hs{};
      for (int a = 0; a < is; a++) {
        for (int b = 0; b < js; b++) {
          float sv = src[ij_to_linear(a + i0, b + j0, cols, 1)];
          float hv = h[ij_to_linear(a + hi0, b + hj0, n, 1)];
          s += sv * hv;
          hs += hv;
        }
      }

      if (norm_h) {
        s /= hs;
      }

      dst[li++] = s;
    }
  }
}

GROVE_NAMESPACE_END
