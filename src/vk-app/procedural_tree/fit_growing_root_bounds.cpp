#include "fit_growing_root_bounds.hpp"
#include "roots_components.hpp"
#include "grove/math/bounds.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace tree;

float max_axis_target_diameter(const TreeRootNode* nodes, int src, int n) {
  assert(src != -1);
  float result{-1.0f};
  int ct{};
  while (src != -1) {
    auto& node = nodes[src];
    result = std::max(result, node.target_diameter);
    src = node.medial_child;
    if (++ct == n) {
      break;
    }
  }
  assert(ct == n);
  return result;
}

void set_axis_max_diameter(ExpandingBoundsSetNode* bs_nodes, const TreeRootNode* root_nodes,
                           int src, int n, float diam) {
  assert(src != -1);
  int ct{};
  while (src != -1) {
    assert(diam >= bs_nodes[src].max_diameter);
    bs_nodes[src].max_diameter = diam;
    src = root_nodes[src].medial_child;
    if (++ct == n) {
      break;
    }
  }
  assert(ct == n);
}

Bounds3f fit_root_axis(const TreeRootNode* nodes, int src, int n, float diam) {
  assert(src != -1);
  Bounds3f result{};
  int ct{};
  while (src != -1) {
    auto& node = nodes[src];
    auto node_obb = make_tree_root_node_obb(node.position, node.direction, node.target_length, diam);
    result = union_of(result, obb3_to_aabb(node_obb));
    src = node.medial_child;
    if (++ct == n) {
      break;
    }
  }
  assert(ct == n);
  return result;
}

void do_update(
  ExpandingBoundsSets& inst, const TreeRootNode* nodes, int num_root_nodes, float diam_scale) {
  //
  const int set_capacity = 4;

  for (auto& entry : inst.entries) {
    entry.modified = false;
  }

  auto& bounds_set_nodes = inst.nodes;
  const int curr_num_bounds_nodes = int(bounds_set_nodes.size());
  assert(curr_num_bounds_nodes <= num_root_nodes);
  const int num_add = num_root_nodes - curr_num_bounds_nodes;
  bounds_set_nodes.resize(bounds_set_nodes.size() + num_add);

  for (int i = 0; i < curr_num_bounds_nodes; i++) {
    if (nodes[i].diameter > bounds_set_nodes[i].max_diameter) {
      inst.entries[bounds_set_nodes[bounds_set_nodes[i].set_root_index].ith_set].modified = true;
    }
  }

  for (int i = curr_num_bounds_nodes; i < num_root_nodes; i++) {
    auto& bs_node = bounds_set_nodes[i];
    bs_node = {};
    if (nodes[i].is_axis_root(i, nodes)) {
      //  need to allocate new set
      bs_node.set_root_index = i;
      bs_node.set_count = 1;
      bs_node.ith_set = uint16_t(inst.entries.size());
      auto& entry = inst.entries.emplace_back();
      entry.modified = true;
    } else {
      const int par = nodes[i].parent;
      assert(par >= 0 && par < int(bounds_set_nodes.size()));
      const int candidate_si = bounds_set_nodes[par].set_root_index;
      auto& candidate_set = bounds_set_nodes[candidate_si];
      if (candidate_set.set_count < set_capacity) {
        bs_node.set_root_index = candidate_si;
        candidate_set.set_count++;
        inst.entries[candidate_set.ith_set].modified = true;
      } else {
        bs_node.set_root_index = i;
        bs_node.set_count = 1;
        bs_node.ith_set = uint16_t(inst.entries.size());
        auto& entry = inst.entries.emplace_back();
        entry.modified = true;
      }
    }
  }

  for (int i = 0; i < num_root_nodes; i++) {
    auto& set_node = bounds_set_nodes[i];
    if (set_node.is_set_root(i) && inst.entries[set_node.ith_set].modified) {
      const int src = set_node.set_root_index;
      const int n = set_node.set_count;
      set_node.max_diameter = max_axis_target_diameter(nodes, src, n) * diam_scale;
      inst.entries[set_node.ith_set].bounds = fit_root_axis(nodes, src, n, set_node.max_diameter);
      set_axis_max_diameter(bounds_set_nodes.data(), nodes, src, n, set_node.max_diameter);
    }
  }

#ifdef GROVE_DEBUG
  validate_expanding_bounds_sets(inst, nodes, num_root_nodes);
#endif
}

} //  anon

void tree::tightly_fit_bounds_sets(
  ExpandingBoundsSets& inst, const TreeRootNode* nodes, int num_nodes) {
  //
  do_update(inst, nodes, num_nodes, 1.0f);
}

void tree::update_expanding_bounds_sets(
  ExpandingBoundsSets& inst, const TreeRootNode* nodes, int num_nodes) {
  //
  do_update(inst, nodes, num_nodes, 2.0f);
}

void tree::validate_expanding_bounds_sets(
  ExpandingBoundsSets& inst, const TreeRootNode* nodes, int num_root_nodes) {
  //
  assert(inst.nodes.size() == size_t(num_root_nodes));
  uint16_t set_ct{};
  for (int i = 0; i < num_root_nodes; i++) {
    auto& set_node = inst.nodes[i];

    if (set_node.is_set_root(i)) {
      assert(set_ct == set_node.ith_set);
      set_ct++;
    }

    auto& node = nodes[i];
    auto& set_root = inst.nodes[set_node.set_root_index];
    assert(set_root.max_diameter >= node.diameter);

    assert(set_root.ith_set < inst.entries.size());
    auto& set_aabb = inst.entries[set_root.ith_set].bounds;
    auto true_obb = make_tree_root_node_obb(
      node.position, node.direction, node.target_length, node.diameter);

    auto true_aabb = obb3_to_aabb(true_obb);
    auto combined_bounds = union_of(true_aabb, set_aabb);
    assert(combined_bounds == set_aabb);
    (void) combined_bounds;
  }
  assert(set_ct == inst.entries.size());
  (void) set_ct;
}

GROVE_NAMESPACE_END
