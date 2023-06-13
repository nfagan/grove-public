#include "utility.hpp"
#include "grove/common/common.hpp"
#include <deque>
#include <unordered_set>

GROVE_NAMESPACE_BEGIN

using namespace tree;

namespace {

using MapFunc = std::function<void(TreeNodeIndex medial_index,
                                   TreeNodeIndex axis_root,
                                   int nth_along_axis,
                                   int axis_size)>;
using SimpleAxisMapFunc = std::function<void(TreeNodeIndex)>;

int compute_axis_size(const Internode* parent, const Internodes& nodes) {
  int size{1};
  while (parent->has_medial_child()) {
    parent = &nodes[parent->medial_child];
    size++;
  }
  return size;
}

template <typename Internodes>
void map_axis(Internodes&& nodes,
              TreeNodeIndex root_ind,
              const MapFunc& medial,
              const MapFunc& lateral,
              bool precompute_axis_size = false) {
  if (root_ind >= int(nodes.size())) {
    assert(root_ind == 0 && nodes.empty());
    return;
  }

  std::deque<tree::TreeNodeIndex> inode_queue;
  inode_queue.push_back(root_ind);

  while (!inode_queue.empty()) {
    auto axis_root_ind = inode_queue.front();
    inode_queue.pop_front();

    auto* axis_root = &nodes[axis_root_ind];
    auto parent = axis_root;
    if (parent->has_lateral_child()) {
      inode_queue.push_back(parent->lateral_child);
    }

    const int axis_size = precompute_axis_size ? compute_axis_size(parent, nodes) : -1;
    int nth_along_axis{1};
    if (lateral) {
      lateral(axis_root_ind, axis_root_ind, 0, axis_size);
    }

    while (parent->has_medial_child()) {
      if (medial) {
        medial(parent->medial_child, axis_root_ind, nth_along_axis, axis_size);
      }

      parent = &nodes[parent->medial_child];
      if (parent->has_lateral_child()) {
        inode_queue.push_back(parent->lateral_child);
      }

      nth_along_axis++;
    }
  }
}

void assign_gravelius_order_impl(tree::Internode* medial, tree::Internode* internodes,
                                 uint16_t grav_order) {
  while (true) {
    medial->gravelius_order = grav_order;
    if (medial->has_lateral_child()) {
      assign_gravelius_order_impl(internodes + medial->lateral_child, internodes, grav_order + 1);
    }
    if (medial->has_medial_child()) {
      medial = internodes + medial->medial_child;
    } else {
      return;
    }
  }
}

} //  anon

int64_t tree::count_num_available_attraction_points(const AttractionPoints& points) {
  int64_t ct{};
  for (auto& node : points.read_nodes()) {
    if (node.data.is_active() && !node.data.is_consumed()) {
      ct++;
    }
  }
  return ct;
}

tree::AxisRootInfo
tree::compute_axis_root_info(const Internodes& internodes, TreeNodeIndex root_index) {
  AxisRootInfo result;

  auto func = [&result, &internodes](auto ind, auto axis_root_ind, int nth_along_axis, int axis_size) {
    auto& node = internodes[ind];
    assert(result.count(node.id) == 0 && nth_along_axis < axis_size);
    InternodeAxisRootInfo info{};
    info.axis_size = axis_size;
    info.nth_along_axis = nth_along_axis;
    info.axis_root_index = axis_root_ind;
    result[node.id] = info;
  };

  grove::map_axis(internodes, root_index, func, func, true);
  return result;
}

void tree::map_axis(const SimpleAxisMapFunc& func, Internodes& internodes,
                    TreeNodeIndex root_index) {
  auto wrap = [&func](auto ind, auto, auto, auto) { func(ind); };
  grove::map_axis(internodes, root_index, wrap, wrap);
}

void tree::map_axis(const SimpleAxisMapFunc& func, const Internodes& internodes,
                    TreeNodeIndex root_index) {
  auto wrap = [&func](auto ind, auto, auto, auto) { func(ind); };
  grove::map_axis(internodes, root_index, wrap, wrap);
}

std::vector<Vec3f> tree::collect_leaf_tip_positions(const Internodes& internodes, int max_num) {
  std::vector<Vec3f> result;
  for (auto& inode : internodes) {
    if (inode.is_leaf()) {
      result.push_back(inode.tip_position());
    }
    if (max_num >= 0 && int(result.size()) >= max_num) {
      break;
    }
  }
  return result;
}

TreeNodeIndex tree::axis_tip_index(const Internodes& internodes, TreeNodeIndex node) {
  auto axis_ind = node;
  while (true) {
    if (internodes[axis_ind].has_medial_child()) {
      axis_ind = internodes[axis_ind].medial_child;
    } else {
      break;
    }
  }
  return axis_ind;
}

int tree::max_gravelius_order(const Internodes& internodes) {
  int max{-1};
  for (auto& node : internodes) {
    if (node.gravelius_order > max) {
      max = node.gravelius_order;
    }
  }
  return max;
}

void tree::reassign_gravelius_order(Internode* internodes, int num_internodes) {
  if (num_internodes > 0) {
    assign_gravelius_order_impl(internodes, internodes, 0);
  }
}

std::vector<Vec3f> tree::extract_octree_points(const AttractionPoints& points) {
  std::vector<Vec3f> result;
  for (auto& node : points.read_nodes()) {
    if (node.is_leaf() && node.data.is_active()) {
      result.push_back(node.data.position);
    }
  }
  return result;
}

std::vector<TreeNodeIndex> tree::collect_medial_indices(const tree::Internode* internodes,
                                                        int num_internodes,
                                                        TreeNodeIndex axis_root_index) {
  std::vector<TreeNodeIndex> result;
  if (axis_root_index < num_internodes) {
    result.push_back(axis_root_index);
    auto* node = internodes + axis_root_index;
    while (node->has_medial_child()) {
      result.push_back(node->medial_child);
      node = internodes + node->medial_child;
    }
  }
  return result;
}

void tree::validate_internode_relationships(const tree::Internodes& internodes) {
  validate_internode_relationships(internodes.data(), int(internodes.size()));
}

void tree::validate_internode_relationships(const tree::Internode* internodes,
                                            int num_internodes) {
  if (num_internodes == 0) {
    return;
  }

  std::unordered_set<tree::TreeInternodeID, tree::TreeInternodeID::Hash> childed;
  for (int i = 0; i < num_internodes; i++) {
    auto& node = internodes[i];
    if (node.has_medial_child()) {
      auto& child = internodes[node.medial_child];
      assert(childed.count(child.id) == 0);
      assert(child.parent == i);
      childed.insert(child.id);
    }
    if (node.has_lateral_child()) {
      auto& child = internodes[node.lateral_child];
      assert(childed.count(child.id) == 0);
      assert(child.parent == i);
      childed.insert(child.id);
    }
  }
  //  Expect all except root internode to be a child.
  assert(
    int(childed.size()) == num_internodes-1 &&
    childed.count(internodes[0].id) == 0);
}

int tree::prune_rejected_axes(const tree::Internode* src, const bool* accepted,
                              int num_src, tree::Internode* dst, int* dst_to_src) {
  const auto make_pending_dst_node = [](tree::Internode res, int parent_ind) {
    res.parent = parent_ind;
    res.medial_child = -1;
    res.lateral_child = -1;
    return res;
  };

  struct AxisInfo {
    int src_self_ind;
    int dst_parent_ind;
  };

  std::vector<AxisInfo> axes;
  if (num_src > 0) {
    axes.push_back({0, -1});
  }

  int num_dst{};
  while (!axes.empty()) {
    const AxisInfo axis_info = axes.back();
    axes.pop_back();
    int src_self_ind = axis_info.src_self_ind;
    int dst_parent_ind = axis_info.dst_parent_ind;
    while (src_self_ind != -1 && accepted[src_self_ind]) {
      const auto& src_node = src[src_self_ind];
      const int dst_self_ind = num_dst++;
      dst[dst_self_ind] = make_pending_dst_node(src_node, dst_parent_ind);

      if (dst_to_src) {
        dst_to_src[dst_self_ind] = src_self_ind;
      }

      if (dst_parent_ind != -1) {
        if (src_self_ind == axis_info.src_self_ind) {
          assert(dst[dst_parent_ind].lateral_child == -1);
          dst[dst_parent_ind].lateral_child = dst_self_ind;
        } else {
          assert(dst[dst_parent_ind].medial_child == -1);
          dst[dst_parent_ind].medial_child = dst_self_ind;
        }
      }
      if (src_node.has_lateral_child()) {
        axes.push_back({src_node.lateral_child, dst_self_ind});
      }

      src_self_ind = src_node.medial_child;
      dst_parent_ind = dst_self_ind;
    }
  }
#ifdef GROVE_DEBUG
  tree::validate_internode_relationships(dst, num_dst);
#endif
  return num_dst;
}

GROVE_NAMESPACE_END
