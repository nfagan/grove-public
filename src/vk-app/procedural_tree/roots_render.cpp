#include "roots_render.hpp"
#include "grove/math/frame.hpp"
#include "grove/math/intersect.hpp"
#include "render.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/common/DynamicArray.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

using namespace tree;

Bounds3f tree::compute_tree_root_node_position_aabb(const TreeRootNode* nodes, int num_nodes) {
  assert(num_nodes <= 2048 && "Allocation required.");
  Temporary<Vec3f, 2048> store_positions;
  Vec3f* positions = store_positions.require(num_nodes);
  for (int i = 0; i < num_nodes; i++) {
    positions[i] = nodes[i].position;
  }
  Bounds3f result;
  union_of(positions, num_nodes, &result.min, &result.max);
  return result;
}

void tree::compute_tree_root_node_frames(const TreeRootNode* nodes, int num_nodes,
                                         TreeRootNodeFrame* dst) {
  if (num_nodes == 0) {
    return;
  }

  TreeRootNodeFrame root{};
  make_coordinate_system_y(nodes[0].direction, &root.i, &root.j, &root.k);
  dst[0] = root;

  for (int i = 0; i < num_nodes; i++) {
    auto& self_frame = dst[i];
    auto& self_node = nodes[i];

    const TreeRootNode* child_node{};
    if (self_node.has_medial_child()) {
      child_node = nodes + self_node.medial_child;
      if (self_node.has_lateral_child()) {
        //  Start of new axis.
        auto& root_frame = dst[self_node.lateral_child];
        root_frame.j = nodes[self_node.lateral_child].direction;
        make_coordinate_system_y(root_frame.j, &root_frame.i, &root_frame.j, &root_frame.k);
      }
    } else if (self_node.has_lateral_child()) {
      child_node = nodes + self_node.lateral_child;
    } else {
      continue;
    }

    auto& child_frame = dst[int(child_node - nodes)];
    child_frame.j = child_node->direction;
    if (std::abs(dot(child_frame.j, self_frame.k)) > 0.99f) {
      make_coordinate_system_y(child_frame.j, &child_frame.i, &child_frame.j, &child_frame.k);
    } else {
      child_frame.i = normalize(cross(child_frame.j, self_frame.k));
      if (dot(child_frame.i, self_frame.i) < 0.0f)  {
        child_frame.i = -child_frame.i;
      }
      child_frame.k = cross(child_frame.i, child_frame.j);
      if (dot(child_frame.k, self_frame.k) < 0.0f) {
        child_frame.k = -child_frame.k;
      }
    }
  }
}

TreeRootRemappedWindAxisRoots
tree::make_tree_root_remapped_wind_axis_roots(const TreeRootNode* nodes, int num_nodes) {
  TreeRootRemappedWindAxisRoots result;

  for (int i = 0; i < num_nodes; i++) {
    const auto& node = nodes[i];
    if (!node.has_parent()) {
      //  Root node is also an axis root.
      result.root_info[i] = node.position;

    } else if (node.is_axis_root(i, nodes)) {
      auto& parent = nodes[node.parent];
      auto obb_parent = make_tree_root_node_obb(parent);

      auto self_ind = i;
      Vec3f axis_position{};
      DynamicArray<int, 256> maybe_remap;

      while (self_ind >= 0) {
        auto& self_node = nodes[self_ind];
        axis_position = self_node.position;
        auto obb_self = make_tree_root_node_obb(self_node);

        if (obb_obb_intersect(obb_self, obb_parent)) {
          maybe_remap.push_back(self_ind);
          self_ind = nodes[self_ind].medial_child;
        } else {
          break;
        }
      }

      if (self_ind < 0) {
        //  All nodes along the child axis intersect with the parent, so do nothing.
      } else {
        //  For each node along the child axis that intersects with the parent, pretend that it
        //  belongs to the parent axis.
        for (auto& remap : maybe_remap) {
          assert(result.evaluate_at.count(remap) == 0);
          result.evaluate_at[remap] = node.parent;
        }
      }

      result.root_info[i] = axis_position;
    }
  }

  //  Any remaining nodes map to themselves.
  for (int i = 0; i < num_nodes; i++) {
    if (result.evaluate_at.count(i) == 0) {
      result.evaluate_at[i] = i;
    }
  }

  return result;
}

TreeRootAxisRootIndices
tree::make_tree_root_axis_root_indices(const TreeRootNode* nodes, int num_nodes) {
  TreeRootAxisRootIndices result;
  if (num_nodes == 0) {
    return result;
  }

  std::vector<int> stack;
  stack.push_back(0);

  while (!stack.empty()) {
    int ni = stack.back();
    stack.pop_back();

    const int axis_root_index = ni;
    while (ni != -1) {
      result.indices[ni] = axis_root_index;
      if (nodes[ni].has_lateral_child()) {
        stack.push_back(nodes[ni].lateral_child);
      }
      ni = nodes[ni].medial_child;
    }
  }

  assert(int(result.indices.size()) == num_nodes);
  return result;
}

WindAxisRootInfo
tree::make_tree_root_wind_axis_root_info(int node_index, const TreeRootNode* nodes,
                                         const TreeRootAxisRootIndices& axis_root_indices,
                                         const TreeRootRemappedWindAxisRoots& remapped_roots,
                                         const Bounds3f& aggregate_aabb) {
  WindAxisRootInfo result{};
  //  axis root0
  Vec4f info_l0{};
  Vec4f info_l1{};
  Vec4f info_l2{};

  if (all(gt(aggregate_aabb.size(), Vec3f{}))) {
    int ni = node_index;
    while (true) {
      auto eval_ni = ni;
      while (true) {
        int next_ni = remapped_roots.evaluate_at.at(eval_ni);
        if (next_ni == eval_ni) {
          break;
        } else {
          eval_ni = next_ni;
        }
      }

      const int axis_root_ind = axis_root_indices.indices.at(eval_ni);
      auto* axis_root_node = nodes + axis_root_ind;
      const Vec3f& axis_position = remapped_roots.root_info.at(axis_root_ind);

      info_l2 = info_l1;
      info_l1 = info_l0;

      info_l0.w = 1.0f; //  active
      auto pos01 = aggregate_aabb.to_fraction(axis_position);
      for (int i = 0; i < 3; i++) {
        assert(std::isfinite(pos01[i]) && pos01[i] >= 0.0f && pos01[i] <= 1.0f);
        info_l0[i] = pos01[i];
      }

      if (axis_root_node->has_parent()) {
        ni = axis_root_node->parent;
      } else {
        break;
      }
    }
  }

  result.info.push_back(info_l0);
  result.info.push_back(info_l1);
  result.info.push_back(info_l2);
  return result;
}

GROVE_NAMESPACE_END
