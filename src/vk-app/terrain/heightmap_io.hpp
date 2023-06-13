#pragma once

#include <memory>
#include <cstddef>

namespace grove {

bool save_height_map(const char* file_path, const float* data, std::size_t num_data, int dim);
std::unique_ptr<float[]> load_height_map(const char* file_path, std::size_t* num_data, int* dim);

}