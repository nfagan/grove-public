#include "roots_utility.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Temporary.hpp"
#include <unordered_set>

GROVE_NAMESPACE_BEGIN

namespace {

using namespace tree;

template <typename Node>
void validate_node_relationships(const Node* nodes, int num_internodes) {
  if (num_internodes == 0) {
    return;
  }

  std::unordered_set<int> childed;
  for (int i = 0; i < num_internodes; i++) {
    auto& node = nodes[i];
    if (node.has_medial_child()) {
      assert(node.medial_child < num_internodes);
      auto& child = nodes[node.medial_child];
      assert(childed.count(node.medial_child) == 0);
      assert(child.parent == i);
      childed.insert(node.medial_child);
    }
    if (node.has_lateral_child()) {
      assert(node.lateral_child < num_internodes);
      auto& child = nodes[node.lateral_child];
      assert(childed.count(node.lateral_child) == 0);
      assert(child.parent == i);
      childed.insert(node.lateral_child);
    }
  }

  //  Expect all except root internode to be a child.
  assert(int(childed.size()) == num_internodes-1 && childed.count(0) == 0);
}

} //  anon

void tree::copy_nodes_applying_node_indices(const TreeRootNode* src_nodes, const int* dst_to_src,
                                            const TreeRootNodeIndices* node_indices,
                                            int num_dst, TreeRootNode* dst_nodes) {
  for (int i = 0; i < num_dst; i++) {
    auto& src = src_nodes[dst_to_src[i]];
    auto& dst = dst_nodes[i];
    auto& ni = node_indices[i];
    dst = src;
    dst.parent = ni.parent;
    dst.medial_child = ni.medial_child;
    dst.lateral_child = ni.lateral_child;
  }
#ifdef GROVE_DEBUG
  validate_node_relationships(dst_nodes, num_dst);
#endif
}

int tree::prune_rejected_axes(const TreeRootNode* src, const bool* accepted,
                              int num_src, TreeRootNodeIndices* dst, int* dst_to_src) {
  const auto make_pending_dst_node = [](int parent_ind) {
    TreeRootNodeIndices res{};
    res.parent = parent_ind;
    res.medial_child = -1;
    res.lateral_child = -1;
    return res;
  };

  struct AxisInfo {
    int src_self_ind;
    int dst_parent_ind;
  };

  Temporary<AxisInfo, 2048> store_stack;
  auto* axes = store_stack.require(num_src);
  int si{};

  if (num_src > 0) {
    axes[si++] = {0, -1};
  }

  int num_dst{};
  while (si > 0) {
    const AxisInfo axis_info = axes[--si];
    int src_self_ind = axis_info.src_self_ind;
    int dst_parent_ind = axis_info.dst_parent_ind;
    while (src_self_ind != -1 && accepted[src_self_ind]) {
      const auto& src_node = src[src_self_ind];
      const int dst_self_ind = num_dst++;
      dst[dst_self_ind] = make_pending_dst_node(dst_parent_ind);

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
        axes[si++] = {src_node.lateral_child, dst_self_ind};
      }

      src_self_ind = src_node.medial_child;
      dst_parent_ind = dst_self_ind;
    }
  }
#ifdef GROVE_DEBUG
  validate_node_relationships(dst, num_dst);
#endif
  return num_dst;
}

GROVE_NAMESPACE_END
