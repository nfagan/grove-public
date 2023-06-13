#include "utility.hpp"
#include "grove/common/common.hpp"
#include <string>

GROVE_NAMESPACE_BEGIN

Optional<double> io::parse_double(const std::string_view& view) {
  try {
    std::size_t v{};
    std::size_t* end_p = &v;
    auto res = std::stod(view.data(), end_p);
    if (end_p && v == view.size()) {
      return Optional<double>(res);
    } else {
      return NullOpt{};
    }
  } catch (...) {
    return NullOpt{};
  }
}

Optional<int64_t> io::parse_int64(const std::string_view& view) {
  try {
    std::size_t v{};
    std::size_t* end_p = &v;
    auto res = std::stoll(view.data(), end_p);
    if (end_p && v == view.size()) {
      return Optional<int64_t>(res);
    } else {
      return NullOpt{};
    }
  } catch (...) {
    return NullOpt{};
  }
}

GROVE_NAMESPACE_END