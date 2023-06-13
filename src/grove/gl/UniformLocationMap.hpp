#pragma once

#include <unordered_map>
#include <string>

namespace grove {
  class UniformLocationMap;
  class Program;
}

class grove::UniformLocationMap {
public:
  UniformLocationMap() = default;
  explicit UniformLocationMap(const Program& program);

  void gather_locations(const Program& program);
  int location(const char* name) const;
  bool has_location(const char* name) const;
  void clear();

private:
  std::unordered_map<std::string, int> uniform_locations;
};