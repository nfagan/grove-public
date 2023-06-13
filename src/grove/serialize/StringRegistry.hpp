#pragma once

#include "common.hpp"
#include <string_view>
#include <string>
#include <cstdint>
#include <unordered_map>
#include <cassert>
#include <vector>

namespace grove::io {

class StringRegistry {
public:
  RegisteredString emplace_view(const std::string_view& view) {
    return emplace(std::string{view});
  }

  RegisteredString emplace(const std::string& str) {
    auto it = registry.find(str);
    if (it == registry.end()) {
      uint64_t next_id = strs.size();
      registry[str] = {next_id};
      strs.push_back(str);
      return {next_id};
    } else {
      return it->second;
    }
  }
  const std::string& get(const RegisteredString& id) const {
    assert(id.id < strs.size());
    return strs[id.id];
  }

private:
  std::unordered_map<std::string, RegisteredString> registry;
  std::vector<std::string> strs;
};

}