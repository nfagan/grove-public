#include "../worley.hpp"
#include "grove/load/image.hpp"
#include <iostream>
#include <string>
#include <chrono>

using namespace grove;
using namespace grove::worley;

int main(int, char**) {
  auto t0 = std::chrono::high_resolution_clock::now();

  const int num_cells = 16;
  const int grid_cell_px = 32;
  const int num_components = 3;

  Parameters params{};
  params.num_cells[0] = num_cells;
  params.num_cells[1] = num_cells;
  params.num_cells[2] = num_cells;
  params.cell_sizes_px[0] = grid_cell_px;
  params.cell_sizes_px[1] = grid_cell_px;
  params.cell_sizes_px[2] = grid_cell_px;

  int px_dims[3];
  get_image_dims_px(params, px_dims);
  size_t num_image_px = num_components * get_image_size_px(px_dims);
  auto image_data = std::make_unique<uint8_t[]>(num_image_px);

  auto num_grid_px = get_sample_grid_size_px(params);
  auto point_grid = std::make_unique<uint8_t[]>(num_grid_px);

  for (int i = 0; i < num_components; i++) {
    generate_sample_grid<uint8_t>(num_grid_px, point_grid.get());
    generate(params, px_dims, point_grid.get(), image_data.get(), num_components, i);
  }

  const std::string dst_file_base = std::string{GROVE_PLAYGROUND_OUT_DIR} + "/worley8";
  for (int i = 0; i < px_dims[2]; i++) {
    auto dst_file = dst_file_base + "-" + std::to_string(i) + ".png";
    write_image(
      image_data.get() + px_dims[0] * px_dims[1] * num_components * i,
      px_dims[1],
      px_dims[0],
      num_components,
      dst_file.c_str());
  }

  auto t1 = std::chrono::high_resolution_clock::now();
  std::cout << "Computed in: "
            << std::chrono::duration<double>(t1 - t0).count() * 1e3
            << "ms"
            << std::endl;

  return 0;
}