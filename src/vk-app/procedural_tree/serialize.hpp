#pragma once

#include "components.hpp"

namespace grove::tree {

std::vector<unsigned char> serialize(const TreeNodeStore& store);
Optional<TreeNodeStore> deserialize(const unsigned char* data, size_t size);

bool serialize_file(const TreeNodeStore& store, const char* file);
Optional<TreeNodeStore> deserialize_file(const char* file);

}