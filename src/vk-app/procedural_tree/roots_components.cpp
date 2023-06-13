#include "roots_components.hpp"
#include "grove/math/frame.hpp"
#include "grove/common/common.hpp"
#include <cassert>

GROVE_NAMESPACE_BEGIN

namespace {

using namespace tree;

} //  anon

TreeRootNode tree::copy_make_tree_root_node(TreeRootNode new_node, int parent,
                                            const Vec3f& dir, const Vec3f& p, float target_length) {
  new_node.parent = parent;
  new_node.medial_child = -1;
  new_node.lateral_child = -1;
  new_node.direction = dir;
  new_node.length = 0.0f;
  new_node.diameter = 0.0f;
  new_node.position = p;
  new_node.target_length = target_length;
  return new_node;
}

TreeRootNode tree::make_tree_root_root_node(const Vec3f& p, const Vec3f& dir,
                                            float target_len, float target_diam) {
  TreeRootNode node0{};
  node0.parent = -1;
  node0.lateral_child = -1;
  node0.medial_child = -1;
  node0.position = p;
  node0.direction = dir;
  node0.target_length = target_len;
  node0.target_diameter = target_diam;
  return node0;
}

TreeRoots tree::make_tree_roots(bounds::RadiusLimiterAggregateID id, int max_num_nodes,
                                const Vec3f& p, const Vec3f& dir, float target_len,
                                float target_diam, float leaf_diam, float diam_power) {
  assert(max_num_nodes > 0);
  TreeRoots result{};
  result.id = id;
  result.origin = p;
  result.max_num_nodes = max_num_nodes;
  result.nodes.resize(max_num_nodes);
  result.node_length_scale = target_len;
  result.leaf_diameter = leaf_diam;
  result.diameter_power = diam_power;
  auto& node0 = result.nodes[result.curr_num_nodes++];
  node0 = make_tree_root_root_node(p, dir, target_len, target_diam);
  return result;
}

OBB3f tree::make_tree_root_node_obb(const Vec3f& p, const Vec3f& dir, float len, float diam) {
  auto r = diam * 0.5f;
  auto half_size_y = len * 0.5f;
  auto position = p + dir * half_size_y;
  OBB3f res{};
  make_coordinate_system_y(dir, &res.i, &res.j, &res.k);
  res.position = position;
  res.half_size = Vec3f{r, half_size_y, r};
  return res;
}

OBB3f tree::make_tree_root_node_obb(const TreeRootNode& node) {
  return make_tree_root_node_obb(
    node.position, node.direction, node.target_length, node.target_diameter);
}

bounds::RadiusLimiterElement
tree::make_tree_root_node_radius_limiter_element(const OBB3f& bounds,
                                                 bounds::RadiusLimiterAggregateID aggregate,
                                                 bounds::RadiusLimiterElementTag tag) {
  assert(bounds.half_size.x == bounds.half_size.z);
  bounds::RadiusLimiterElement res{};
  res.i = bounds.i;
  res.j = bounds.j;
  res.k = bounds.k;
  res.p = bounds.position;
  res.half_length = bounds.half_size.y;
  res.radius = bounds.half_size.x;
  res.aggregate_id = aggregate;
  res.tag = tag;
  return res;
}

GROVE_NAMESPACE_END
