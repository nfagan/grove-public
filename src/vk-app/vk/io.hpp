#pragma once

#include "grove/common/Optional.hpp"
#include <vector>
#include <string>

namespace grove::io {

Optional<std::vector<uint32_t>> read_spv(const std::string& filename);

}