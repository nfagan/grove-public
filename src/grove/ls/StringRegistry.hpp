#pragma once

#include "common.hpp"
#include <unordered_map>
#include <vector>
#include <cassert>

namespace grove::ls {

class StringRegistry {
public:
  StringRef emplace_view(const std::string_view& view) {
    return emplace(std::string{view});
  }

  StringRef emplace(const std::string& str) {
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
  const std::string& get(const StringRef& id) const {
    assert(id.id < strs.size());
    return strs[id.id];
  }

private:
  std::unordered_map<std::string, StringRef> registry;
  std::vector<std::string> strs;
};

}