#include "environment_sample.hpp"
#include "grove/common/common.hpp"
#include "grove/math/util.hpp"
#include <deque>
#include <unordered_set>

GROVE_NAMESPACE_BEGIN

namespace {

struct BudQInfo {
  float q;
};

struct BudQMeta {
  int index{};
  float weight{};
};

struct SetAxisQResult {
  std::vector<tree::TreeNodeIndex> axis_roots;
  float total_q{};
};

using ViewBuds = ArrayView<tree::Bud>;
using ViewInternodes = ArrayView<tree::Internode>;

float sum_weighted_q(const std::vector<BudQInfo>& bud_info,
                     const std::vector<BudQMeta>& bud_meta) {
  float q{};
  for (int i = 0; i < int(bud_info.size()); i++) {
    q += bud_info[i].q * bud_meta[i].weight;
  }
  return q == 0.0f ? 1.0f : q;
}

float sum_bud_q(const tree::Internode& inode, const ViewBuds& buds) {
  float q{};
  for (int i = 0; i < inode.num_buds; i++) {
    q += buds[inode.bud_indices[i]].q;
  }
  return q;
}

#ifdef GROVE_DEBUG
int count_terminal_buds(const tree::Internode& inode, const ViewBuds& buds) {
  int ct{};
  for (int i = 0; i < inode.num_buds; i++) {
    ct += buds[inode.bud_indices[i]].is_terminal ? 1 : 0;
  }
  return ct;
}
#endif

#ifdef GROVE_DEBUG
void check_visited_all_terminal_buds(const ViewBuds& buds, int num_visited) {
  int tot_terms{};
  for (auto& bud : buds) {
    if (bud.is_terminal) {
      tot_terms++;
    }
  }
  assert(tot_terms == num_visited);
}
#endif

void push_bud_info(const tree::Internode& node, ViewBuds& buds,
                   std::vector<BudQInfo>& bud_info,
                   std::vector<BudQMeta>& bud_meta,
                   tree::TreeNodeIndex& term_bud_ind) {
  for (int i = 0; i < node.num_buds; i++) {
    auto bud_ind = node.bud_indices[i];
    auto& bud = buds[bud_ind];

    bud_info.push_back({bud.q});
    bud_meta.emplace_back();

    if (bud.is_terminal) {
      assert(term_bud_ind == -1);
      term_bud_ind = bud_ind;
    }
  }
}

void gather_bud_qs(const tree::Internode& root_node,
                   ViewBuds& buds,
                   const ViewInternodes& internodes,
                   std::vector<BudQInfo>& bud_info,
                   std::vector<BudQMeta>& bud_meta) {
  tree::TreeNodeIndex term_bud_ind{-1};

  //  Gather bud qs for this axis.
  push_bud_info(root_node, buds, bud_info, bud_meta, term_bud_ind);
  if (root_node.has_lateral_child()) {
    bud_info.push_back({root_node.lateral_q});
    bud_meta.emplace_back();
  }

  auto* node = &root_node;
  while (node->has_medial_child()) {
    node = &internodes[node->medial_child];
    push_bud_info(*node, buds, bud_info, bud_meta, term_bud_ind);

    if (node->has_lateral_child()) {
      bud_info.push_back({node->lateral_q});
      bud_meta.emplace_back();
    }
  }
}

void compute_resource_weights(std::vector<BudQMeta>& bud_meta,
                              const std::vector<BudQInfo>& bud_info,
                              const tree::DistributeBudQParams& params) {
  assert(bud_meta.size() == bud_info.size());

  int ind{};
  for (auto& meta : bud_meta) {
    meta.index = ind++;
  }

  std::sort(bud_meta.begin(), bud_meta.end(), [&bud_info](auto&& a, auto&& b) {
    //  Descend.
    return bud_info[a.index].q > bud_info[b.index].q;
  });

  //  Scale resource, piece-wise linear over number of buds.
  for (auto& meta : bud_meta) {
    auto t = std::min(1.0f, float(meta.index) / (params.k * float(bud_meta.size())));
    meta.weight = grove::lerp(t, params.w_max, params.w_min);
  }
}

void set_bud_q(ViewBuds buds, const tree::EnvironmentInputs& inputs) {
  for (auto& bud : buds) {
    if (auto it = inputs.find(bud.id); it != inputs.end()) {
      bud.q = it->second.q;
    } else {
      bud.q = 0.0f;
    }
  }
}

void distribute_bud_q(ViewBuds buds, const ViewInternodes& internodes,
                      const ArrayView<tree::TreeNodeIndex>& axis_roots, float total_q,
                      const tree::DistributeBudQParams& params) {
#ifdef GROVE_DEBUG
  std::unordered_set<tree::TreeBudID, tree::TreeBudID::Hash> visited_buds;
#endif

  std::unordered_map<tree::TreeNodeIndex, float> branch_vs;
  if (!axis_roots.empty()) {
    branch_vs[axis_roots[0]] = params.resource_scalar * total_q;
  }

  std::vector<BudQInfo> bud_info;
  std::vector<BudQMeta> bud_meta;

  for (auto& root_ind : axis_roots) {
    auto& root_node = internodes[root_ind];

    gather_bud_qs(root_node, buds, internodes, bud_info, bud_meta);
    compute_resource_weights(bud_meta, bud_info, params);

    //  Distribute the resource among buds.
    auto normalize_q = sum_weighted_q(bud_info, bud_meta);
    auto branch_v = branch_vs.at(root_ind);
    int m_ind{};
    for (int i = 0; i < root_node.num_buds; i++) {
      auto& bud = buds[root_node.bud_indices[i]];
      bud.v = branch_v * (bud_meta[m_ind++].weight * bud.q) / normalize_q;
#ifdef GROVE_DEBUG
      assert(visited_buds.count(bud.id) == 0);
      visited_buds.insert(bud.id);
#endif
    }

    if (root_node.has_lateral_child()) {
      assert(branch_vs.count(root_node.lateral_child) == 0);
      auto q = root_node.lateral_q;
      auto v = branch_v * (bud_meta[m_ind++].weight * q) / normalize_q;
      branch_vs[root_node.lateral_child] = v;
    }

    auto* node = &root_node;
    while (node->has_medial_child()) {
      node = &internodes[node->medial_child];
      for (int i = 0; i < node->num_buds; i++) {
        auto& bud = buds[node->bud_indices[i]];
        bud.v = branch_v * (bud_meta[m_ind++].weight * bud.q) / normalize_q;
#ifdef GROVE_DEBUG
        assert(visited_buds.count(bud.id) == 0);
        visited_buds.insert(bud.id);
#endif
      }
      if (node->has_lateral_child()) {
        assert(branch_vs.count(node->lateral_child) == 0);
        auto q = node->lateral_q;
        auto v = branch_v * (bud_meta[m_ind++].weight * q) / normalize_q;
        branch_vs[node->lateral_child] = v;
      }
    }
    assert(m_ind == int(bud_meta.size()));
    //  Clear for next iteration.
    bud_info.clear();
    bud_meta.clear();
  }

#ifdef GROVE_DEBUG
  for (auto& bud : buds) {
    assert(visited_buds.count(bud.id) > 0);
  }
#endif
}

SetAxisQResult set_axis_q(ViewInternodes internodes, const ViewBuds& buds,
                          tree::TreeNodeIndex root_inode_index) {
  for (auto& inode : internodes) {
    inode.lateral_q = 0.0f;
  }

  SetAxisQResult result;

  std::deque<tree::TreeNodeIndex> inode_queue;
  inode_queue.push_back(root_inode_index);

  int num_visited_terminal_buds{};
  float total_q{};

  while (!inode_queue.empty()) {
    auto axis_root_ind = inode_queue.front();
    inode_queue.pop_front();
    result.axis_roots.push_back(axis_root_ind);

    auto* axis_root = &internodes[axis_root_ind];
    auto parent = axis_root;

    if (parent->has_lateral_child()) {
      inode_queue.push_back(parent->lateral_child);
    }

    auto bud_count = parent->num_buds;
    auto bud_qs = sum_bud_q(*parent, buds);

    while (parent->has_medial_child()) {
      assert(int(bud_count) + int(parent->num_buds) <= 127);
      parent = &internodes[parent->medial_child];
      bud_count += parent->num_buds;
      bud_qs += sum_bud_q(*parent, buds);

      if (parent->has_lateral_child()) {
        inode_queue.push_back(parent->lateral_child);
      }
    }

    assert(parent->num_buds > 0);
    assert(count_terminal_buds(*parent, buds) == 1);
    num_visited_terminal_buds++;

    auto mean_q = bud_count > 0 ? bud_qs / float(bud_count) : 0.0f;
    if (axis_root->has_parent()) {
      assert(internodes[axis_root->parent].lateral_q == 0.0f);
      assert(internodes[axis_root->parent].lateral_child == axis_root_ind);
      internodes[axis_root->parent].lateral_q = mean_q;
    }

    total_q += bud_qs;
  }

#ifdef GROVE_DEBUG
  check_visited_all_terminal_buds(buds, num_visited_terminal_buds);
#endif

  result.total_q = total_q;
  return result;
}

} //  anon

void tree::apply_environment_input(ArrayView<Bud> buds,
                                   const ArrayView<Internode>& internodes,
                                   TreeNodeIndex root_inode_index,
                                   const EnvironmentInputs& inputs,
                                   const DistributeBudQParams& params) {
  set_bud_q(buds, inputs);
  auto axis_q_res = set_axis_q(internodes, buds, root_inode_index);
  auto axis_roots = make_data_array_view<TreeNodeIndex>(axis_q_res.axis_roots);
  distribute_bud_q(buds, internodes, axis_roots, axis_q_res.total_q, params);
}

void tree::apply_environment_input(TreeNodeStore& tree_nodes,
                                   const EnvironmentInputs& inputs,
                                   const DistributeBudQParams& params) {
  auto buds = make_data_array_view<Bud>(tree_nodes.buds);
  auto internodes = make_data_array_view<Internode>(tree_nodes.internodes);
  TreeNodeIndex root_inode_index{0};
  apply_environment_input(buds, internodes, root_inode_index, inputs, params);
}

GROVE_NAMESPACE_END
