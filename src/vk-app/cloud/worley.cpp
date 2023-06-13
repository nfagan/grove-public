#include "worley.hpp"
#include "grove/common/common.hpp"
#include "grove/math/random.hpp"

GROVE_NAMESPACE_BEGIN

float worley::DefaultRandom::evaluate() {
  return urandf();
}

size_t worley::get_sample_grid_size(const Parameters& params) {
  size_t prod{1};
  for (int c : params.num_cells) {
    prod *= size_t(c);
  }
  return prod;
}

size_t worley::get_sample_grid_size_px(const Parameters& params) {
  return get_sample_grid_size(params) * 3;
}

size_t worley::get_image_size_px(const int px_dims[3]) {
  return size_t(px_dims[0]) * size_t(px_dims[1]) * size_t(px_dims[2]);
}

void worley::get_image_dims_px(const Parameters& params, int out[3]) {
  for (int i = 0; i < 3; i++) {
    out[i] = params.num_cells[i] * params.cell_sizes_px[i];
  }
}

float worley::maximum_pixel_distance(const Parameters& params) {
  auto ry = float(params.cell_sizes_px[0]);
  auto rx = float(params.cell_sizes_px[1]);
  auto rz = float(params.cell_sizes_px[2]);
  return std::sqrt(rx * rx + ry * ry + rz * rz);
}

GROVE_NAMESPACE_END
