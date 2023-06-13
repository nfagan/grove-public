#pragma once

#include "grove/math/vector.hpp"
#include "grove/common/Optional.hpp"
#include <vector>
#include <string>

namespace grove::tree::io {

struct Node {
  Vec3f position;
  Vec3f direction;
  float diameter;
  float length;
  int parent;
  int medial_child;
  int lateral_child;
};

Optional<std::vector<Node>> deserialize(const std::string& file_path);
bool serialize(const Node* nodes, int num_nodes, const std::string& file_path);

}