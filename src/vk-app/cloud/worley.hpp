#pragma once

#include "grove/math/constants.hpp"
#include <cstdint>
#include <cmath>

namespace grove::worley {

struct Parameters {
  int cell_sizes_px[3];
  int num_cells[3];
  bool invert;
};

struct DefaultRandom {
  static float evaluate();
};

size_t get_sample_grid_size(const Parameters& params);
size_t get_sample_grid_size_px(const Parameters& params);
size_t get_image_size_px(const int px_dims[3]);
void get_image_dims_px(const Parameters& params, int out[3]);
float maximum_pixel_distance(const Parameters& params);

inline int to_linear_index(int i, int j, int k, const int dims[3]) {
  int slab = k * dims[0] * dims[1];
  int within_slab = i * dims[1] + j;
  return within_slab + slab;
}

inline void get_cell_index(const int* px_coord, const int* cell_size, int* out, int size) {
  for (int i = 0; i < size; i++) {
    out[i] = px_coord[i] / cell_size[i];
  }
}

namespace impl {

template <typename T>
struct FloatConversion {
  //
};

template <>
struct FloatConversion<uint8_t> {
  static float to_float01(uint8_t value) {
    return float(value) / 255.0f;
  }
  static uint8_t from_float01(float v) {
    return uint8_t(v * 255.0f);
  }
};

template <>
struct FloatConversion<float> {
  static float to_float01(float v) {
    return v;
  }
  static float from_float01(float v) {
    return v;
  }
};

template <typename Element>
Element min_distance(const Element* point_grid,
                     const int cell_size[3],
                     const int num_cells[3],
                     const int px_coord[3],
                     float max_dist,
                     bool invert) {
  int cell_ind[3];
  get_cell_index(px_coord, cell_size, cell_ind, 3);
  float min_dist = infinityf();

  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      for (int k = 0; k < 3; k++) {
        int offs[3] = {i - 1, j - 1, k - 1};
        int true_ind[3];
        int sample_ind[3];
        for (int h = 0; h < 3; h++) {
          int adj_ind = offs[h] + cell_ind[h];
          true_ind[h] = adj_ind;
          sample_ind[h] = adj_ind < 0 ? num_cells[h] - 1 : adj_ind >= num_cells[h] ? 0 : adj_ind;
        }
        const int linear_grid_ind = 3 * to_linear_index(
          sample_ind[0], sample_ind[1], sample_ind[2], num_cells);

        float len_sq = 0.0f;
        for (int h = 0; h < 3; h++) {
          const Element sample = point_grid[linear_grid_ind + h];
          const float samplef = impl::FloatConversion<Element>::to_float01(sample);
          const float px_sample =
            samplef * float(cell_size[h]) + float(cell_size[h]) * float(true_ind[h]);
          float component = px_sample - float(px_coord[h]);
          len_sq += component * component;
        }

        float dist = std::sqrt(len_sq);
        if (dist < min_dist) {
          min_dist = dist;
        }
      }
    }
  }
  if (min_dist > max_dist) {
    min_dist = max_dist;
  }
  float normed = min_dist / max_dist;
  if (invert) {
    normed = 1.0f - normed;
  }
  return impl::FloatConversion<Element>::from_float01(normed);
}

} //  impl

template <typename Element, typename Random = DefaultRandom>
void generate_sample_grid(size_t num_px, Element* out_point_grid) {
  for (size_t i = 0; i < num_px; i++) {
    out_point_grid[i] = impl::FloatConversion<Element>::from_float01(Random::evaluate());
  }
}

template <typename Element>
void generate(const Parameters& params,
              const int px_dims[3],
              const Element* point_grid,
              Element* dst,
              size_t dst_stride,
              size_t dst_offset) {
  const float max_dist = maximum_pixel_distance(params);

  size_t dst_ind{};
  for (int k = 0; k < px_dims[2]; k++) {
    for (int i = 0; i < px_dims[0]; i++) {
      for (int j = 0; j < px_dims[1]; j++) {
        const int px_coord[3] = {i, j, k};
        size_t dst_write = (dst_ind++) * dst_stride + dst_offset;
        dst[dst_write] = impl::min_distance<Element>(
          point_grid,
          params.cell_sizes_px,
          params.num_cells,
          px_coord,
          max_dist,
          params.invert);
      }
    }
  }
}

}