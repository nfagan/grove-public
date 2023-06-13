#pragma once

#include "grove/math/Bounds3.hpp"
#include <vector>

namespace grove::tree {

struct TreeRootNode;

struct ExpandingBoundsSetNode {
  bool is_set_root(int i) const {
    return set_root_index == i;
  }

  int set_root_index;
  uint16_t set_count;
  uint16_t ith_set;
  float max_diameter;
};

struct ExpandingBoundsSets {
public:
  struct Entry {
    Bounds3f bounds;
    bool modified{};
  };

public:
  int num_entries() const {
    return int(entries.size());
  }
  void clear() {
    nodes.clear();
    entries.clear();
  }

public:
  std::vector<ExpandingBoundsSetNode> nodes;
  std::vector<Entry> entries;
};

void update_expanding_bounds_sets(
  ExpandingBoundsSets& inst, const tree::TreeRootNode* nodes, int num_nodes);

void tightly_fit_bounds_sets(
  ExpandingBoundsSets& inst, const tree::TreeRootNode* nodes, int num_nodes);

void validate_expanding_bounds_sets(
  ExpandingBoundsSets& inst, const tree::TreeRootNode* nodes, int num_nodes);

}