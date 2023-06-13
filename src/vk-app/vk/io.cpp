#include "io.hpp"
#include "grove/common/common.hpp"
#include <fstream>
#include <cmath>

GROVE_NAMESPACE_BEGIN

Optional<std::vector<uint32_t>> io::read_spv(const std::string& filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);
  if (!file.good()) {
    return NullOpt{};
  } else {
    size_t file_size = (size_t) file.tellg();
    //  To uint32
    const auto align_size = int64_t(std::ceil(double(file_size) / double(sizeof(uint32_t))));
    std::vector<uint32_t> buffer(align_size);
    std::fill(buffer.begin(), buffer.end(), 0);
    //  Read actual file size.
    file.seekg(0);
    file.read((char*) buffer.data(), int64_t(file_size));
    file.close();
    return Optional<std::vector<uint32_t>>(std::move(buffer));
  }
}

GROVE_NAMESPACE_END
