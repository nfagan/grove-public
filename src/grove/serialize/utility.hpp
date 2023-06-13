#pragma once

#include "grove/common/Optional.hpp"
#include <string_view>

namespace grove::io {

Optional<double> parse_double(const std::string_view& view);
Optional<int64_t> parse_int64(const std::string_view& view);

}