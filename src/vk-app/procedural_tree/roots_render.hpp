#pragma once

#include "roots_components.hpp"
#include "grove/math/Bounds3.hpp"
#include <unordered_map>

namespace grove::tree {

struct TreeRootNodeFrame {
  Vec3f i;
  Vec3f j;
  Vec3f k;
};

struct TreeRootRemappedWindAxisRoots {
  std::unordered_map<int, Vec3f> root_info;
  std::unordered_map<int, int> evaluate_at;
};

struct TreeRootAxisRootIndices {
  std::unordered_map<int, int> indices;
};

void compute_tree_root_node_frames(const TreeRootNode* nodes, int num_nodes, TreeRootNodeFrame* dst);
Bounds3f compute_tree_root_node_position_aabb(const TreeRootNode* nodes, int num_nodes);

TreeRootRemappedWindAxisRoots
make_tree_root_remapped_wind_axis_roots(const TreeRootNode* nodes, int num_nodes);

TreeRootAxisRootIndices make_tree_root_axis_root_indices(const TreeRootNode* nodes, int num_nodes);

struct WindAxisRootInfo;
WindAxisRootInfo make_tree_root_wind_axis_root_info(int node_index, const TreeRootNode* nodes,
                                                    const TreeRootAxisRootIndices& axis_root_indices,
                                                    const TreeRootRemappedWindAxisRoots& remapped_roots,
                                                    const Bounds3f& aggregate_aabb);

}