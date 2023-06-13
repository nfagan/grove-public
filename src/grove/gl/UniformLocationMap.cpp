#include "UniformLocationMap.hpp"
#include "Program.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"

GROVE_NAMESPACE_BEGIN

namespace {
#if GROVE_LOGGING_ENABLED
  void warn_unrecognized_uniform(const char* name) {
    auto warn_message = std::string("No such uniform: ") + name;
    GROVE_LOG_WARNING_CAPTURE_META(warn_message.c_str(), "UniformLocationMap");
  }
#endif
}

UniformLocationMap::UniformLocationMap(const Program& program) {
  gather_locations(program);
}

int UniformLocationMap::location(const char* name) const {
  const auto it = uniform_locations.find(name);

  if (it == uniform_locations.end()) {
#if GROVE_LOGGING_ENABLED
    warn_unrecognized_uniform(name);
#endif
    return -1;
  }

  return it->second;
}

bool UniformLocationMap::has_location(const char* name) const {
  return uniform_locations.count(name) > 0;
}

void UniformLocationMap::gather_locations(const Program& program) {
  const std::vector<std::string> names = program.active_uniform_names();

  for (const auto& name : names) {
    uniform_locations.emplace(name, program.uniform_location(name.c_str()));
  }
}

void UniformLocationMap::clear() {
  uniform_locations.clear();
}

GROVE_NAMESPACE_END