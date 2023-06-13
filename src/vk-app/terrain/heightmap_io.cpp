#include "heightmap_io.hpp"
#include "grove/common/common.hpp"
#include <fstream>
#include <iostream>
#include <cassert>

GROVE_NAMESPACE_BEGIN

std::unique_ptr<float[]> load_height_map(const char* file_path,
                                         std::size_t* num_data,
                                         int* dim) {
  std::ifstream file;
  file.open(file_path, std::ios_base::in | std::ios_base::binary);

  if (!file.good()) {
    return nullptr;
  }

  file.seekg(0, file.end);
  const std::size_t file_size = file.tellg();
  file.seekg(0, file.beg);

  auto header_size = sizeof(std::size_t) + sizeof(int);

  if (file_size < header_size) {
    return nullptr;
  }

  file.read((char*) dim, sizeof(int));
  file.read((char*) num_data, sizeof(std::size_t));

  auto num_elements = *num_data;
  auto actual_dim = *dim;
  auto data_size = sizeof(float) * num_elements;

  if (data_size + header_size != file_size ||
      actual_dim * actual_dim != int(num_elements)) {
    return nullptr;
  }

  auto res = std::unique_ptr<float[]>(new float[num_elements]);
  file.read((char*) res.get(), sizeof(float) * num_elements);

  return res;
}

bool save_height_map(const char* file_path,
                     const float* data,
                     std::size_t num_data,
                     int dim) {
  assert(int(num_data) == dim * dim);

  std::ofstream wf(file_path, std::ios::out | std::ios::binary);

  if (!wf.good()) {
    return false;
  }

  wf.write((char*) &dim, sizeof(int));
  wf.write((char*) &num_data, sizeof(std::size_t));
  wf.write((char*) data, num_data * sizeof(float));

  return wf.good();
}

GROVE_NAMESPACE_END
