#include "array.hpp"
#include "grove/common/common.hpp"
#include <fstream>

GROVE_NAMESPACE_BEGIN

std::unique_ptr<float[]> io::read_float_array(const char* file_path, bool* success,
                                              uint64_t* num_elements) {
  *success = false;

  std::ifstream file;
  file.open(file_path, std::ios_base::in | std::ios_base::binary);

  if (!file.good()) {
    return nullptr;
  }

  file.seekg(0, file.end);
  const std::size_t file_size = file.tellg();
  file.seekg(0, file.beg);

  auto header_size = sizeof(uint64_t);
  if (file_size < header_size) {
    return nullptr;
  }

  file.read((char*) num_elements, sizeof(uint64_t));

  auto num_els = *num_elements;
  auto data_size = sizeof(float) * num_els;

  if (data_size + header_size != file_size) {
    return nullptr;
  }

  auto res = std::make_unique<float[]>(num_els);
  file.read((char*) res.get(), data_size);
  *success = true;

  return res;
}

GROVE_NAMESPACE_END
