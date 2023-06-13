#pragma once

#include "radius_limiter.hpp"
#include <vector>
#include <unordered_set>

namespace grove::tree {

struct TreeRootNode {
  bool has_medial_child() const {
    return medial_child >= 0;
  }
  bool has_lateral_child() const {
    return lateral_child >= 0;
  }
  bool has_parent() const {
    return parent >= 0;
  }
  float target_radius() const {
    return target_diameter * 0.5f;
  }
  bool is_axis_root(int self, const TreeRootNode* nodes) const {
    return !has_parent() || nodes[parent].lateral_child == self;
  }
  bool is_axis_tip() const {
    return !has_medial_child();
  }
  Vec3f tip_position() const {
    return position + direction * length;
  }

  int parent;
  int medial_child;
  int lateral_child;
  Vec3f direction;
  Vec3f position;
  float length;
  float target_length;
  float diameter;
  float target_diameter;
};

struct TreeRootNodeIndices {
  bool has_parent() const {
    return parent >= 0;
  }
  bool has_medial_child() const {
    return medial_child >= 0;
  }
  bool has_lateral_child() const {
    return lateral_child >= 0;
  }

  int parent;
  int medial_child;
  int lateral_child;
};

struct TreeRoots {
  bounds::RadiusLimiterAggregateID id{};
  Vec3f origin{};
  int max_num_nodes{};
  int curr_num_nodes{};
  float node_length_scale{};
  float leaf_diameter{};
  float diameter_power{};
  std::vector<TreeRootNode> nodes;
};

struct GrowingTreeRootNode {
  int index;
  bool finished;
};

struct TreeRootsGrowthContext {
  std::vector<GrowingTreeRootNode> growing;
};

using TreeRootsSkipReceding = std::unordered_set<int>;

struct TreeRootsRecedeContext {
  std::vector<uint16_t> node_orders;
  std::vector<GrowingTreeRootNode> receding;
  int num_pending_axis_roots{};
  const TreeRootsSkipReceding* skip{};
};

inline GrowingTreeRootNode make_growing_tree_root_node(int index) {
  GrowingTreeRootNode res{};
  res.index = index;
  return res;
}

TreeRootNode make_tree_root_root_node(const Vec3f& p, const Vec3f& dir,
                                      float target_len, float target_diam);

TreeRootNode copy_make_tree_root_node(TreeRootNode new_node, int parent,
                                      const Vec3f& dir, const Vec3f& p, float target_length);

TreeRoots make_tree_roots(bounds::RadiusLimiterAggregateID id, int max_num_nodes,
                          const Vec3f& p, const Vec3f& dir,
                          float target_len, float target_diam, float leaf_diam, float diam_power);

OBB3f make_tree_root_node_obb(const Vec3f& p, const Vec3f& dir, float len, float diam);
OBB3f make_tree_root_node_obb(const TreeRootNode& node);

bounds::RadiusLimiterElement
make_tree_root_node_radius_limiter_element(const OBB3f& bounds,
                                           bounds::RadiusLimiterAggregateID aggregate,
                                           bounds::RadiusLimiterElementTag tag);

}