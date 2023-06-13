#include "collision_with_roots.hpp"
#include "geometry.hpp"
#include "../procedural_tree/roots_utility.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Temporary.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace arch;
using namespace tree;

void gather_candidates(const ComputeWallHolesAroundRootsParams& params,
                       std::vector<const TreeRoots*>& candidate_roots,
                       std::vector<RootsInstanceHandle>& candidate_handles) {
  for (auto& id : params.intersect_result->hit_root_aggregate_ids) {
    auto handle = lookup_roots_instance_by_radius_limiter_aggregate_id(params.roots_system, id);
    if (handle) {
      auto inst = read_roots_instance(params.roots_system, handle.value());
      if (inst.roots) {
        candidate_roots.push_back(inst.roots);
        candidate_handles.push_back(handle.value());
      }
    }
  }
}

void make_internodes_for_collision(const TreeRootNode* root_nodes, int num_nodes, Internode* dst) {
  for (int i = 0; i < num_nodes; i++) {
    const auto& src = root_nodes[i];
    Internode node{};
    node.id = TreeInternodeID{uint32_t(i)};
    node.parent = src.parent;
    node.medial_child = src.medial_child;
    node.lateral_child = src.lateral_child;
    node.position = src.position;
    node.render_position = src.position;
    node.direction = src.direction;
    node.length = src.target_length;
    node.length_scale = 1.0f;
    node.diameter = src.target_diameter;
    dst[i] = node;
  }
}

void extract_node_indices(const Internode* nodes, int num_nodes, TreeRootNodeIndices* dst) {
  for (int i = 0; i < num_nodes; i++) {
    TreeRootNodeIndices tmp_ni{};
    tmp_ni.parent = nodes[i].parent;
    tmp_ni.medial_child = nodes[i].medial_child;
    tmp_ni.lateral_child = nodes[i].lateral_child;
    dst[i] = tmp_ni;
  }
}

void reject_intersecting(const TreeRootNode* nodes, int num_nodes, const OBB3f& bounds, bool* accept) {
  for (int ni = 0; ni < num_nodes; ni++) {
    auto node_obb = tree::make_tree_root_node_obb(nodes[ni]);
    if (obb_obb_intersect(bounds, node_obb)) {
      accept[ni] = false;
    } else {
      accept[ni] = true;
    }
  }
}

} //  anon

RootBoundsIntersectResult
arch::root_bounds_intersect(const bounds::RadiusLimiter* lim, const OBB3f& bounds,
                            bounds::RadiusLimiterElementTag roots_tag,
                            bounds::RadiusLimiterElementTag tree_tag,
                            const Optional<bounds::RadiusLimiterAggregateID>& allow_element) {
  RootBoundsIntersectResult result{};

  std::vector<bounds::RadiusLimiterElement> hit_elems;
  bounds::gather_intersecting(lim, bounds, hit_elems);

  auto& hit_ids = result.hit_root_aggregate_ids;
  for (auto& elem : hit_elems) {
    if (elem.tag == roots_tag) {
      auto it = std::find(hit_ids.begin(), hit_ids.end(), elem.aggregate_id);
      if (it == hit_ids.end()) {
        result.hit_root_aggregate_ids.push_back(elem.aggregate_id);
      }
    } else if (elem.tag != tree_tag) {
      if (!allow_element || allow_element.value() != elem.aggregate_id) {
        result.any_hit_besides_tree_or_roots = true;
      }
    }
  }

  result.any_hit_roots = !hit_ids.empty();
  return result;
}

bool arch::can_prune_all_candidates(const tree::RootsSystem* sys,
                                    const RootBoundsIntersectResult& result) {
  //  @NOTE: Ignores non-existent candidates.
  for (auto& id : result.hit_root_aggregate_ids) {
    auto handle = tree::lookup_roots_instance_by_radius_limiter_aggregate_id(sys, id);
    if (handle && !tree::can_start_pruning(sys, handle.value())) {
      return false;
    }
  }
  return true;
}

ComputeWallHolesAroundRootsResult
arch::compute_wall_holes_around_roots(const ComputeWallHolesAroundRootsParams& params) {
  ComputeWallHolesAroundRootsResult result{};

  std::vector<const tree::TreeRoots*> candidate_roots;
  std::vector<tree::RootsInstanceHandle> candidate_handles;
  gather_candidates(params, candidate_roots, candidate_handles);
  assert(candidate_handles.size() == candidate_roots.size());

  if (candidate_roots.empty()) {
    return result;
  } else {
    result.pruned_instances = std::move(candidate_handles);
    result.pruned_dst_to_src.resize(candidate_roots.size());
    result.pruned_node_indices.resize(candidate_roots.size());
  }

  const bool try_compute_holes = params.collide_through_hole_params != nullptr;
  int max_num_found_holes_ind{};
  int max_num_found_holes{-1};

  if (try_compute_holes) {
    constexpr int max_num_wall_holes = 4;
    using WallHoleArray = std::array<arch::WallHole, max_num_wall_holes>;

    std::vector<WallHoleArray> candidate_wall_holes(candidate_roots.size());
    for (int i = 0; i < int(candidate_roots.size()); i++) {
      const tree::TreeRoots& roots = *candidate_roots[i];

      const int num_src_nodes = roots.curr_num_nodes;
      Temporary<tree::Internode, 2048> tmp_internodes;
      tree::Internode* src_nodes = tmp_internodes.require(num_src_nodes);
      make_internodes_for_collision(roots.nodes.data(), num_src_nodes, src_nodes);

      TreeNodeCollisionWithWallParams collide_params{};
      collide_params.collision_context = params.collision_context;
      collide_params.collide_through_hole_params = params.collide_through_hole_params;
      collide_params.wall_bounds = params.wall_bounds;
      collide_params.src_internodes = src_nodes;
      collide_params.num_src_internodes = num_src_nodes;
      collide_params.accepted_holes = candidate_wall_holes[i].data();
      collide_params.max_num_accepted_holes = max_num_wall_holes;
      auto collide_res = compute_collision_with_wall(collide_params);

      if (collide_res.num_accepted_bounds_components > max_num_found_holes) {
        max_num_found_holes = collide_res.num_accepted_bounds_components;
        max_num_found_holes_ind = i;
      }

      auto& dst_to_src = result.pruned_dst_to_src[i];
      dst_to_src.resize(collide_res.num_dst_internodes);
      std::copy(
        collide_res.dst_to_src,
        collide_res.dst_to_src + collide_res.num_dst_internodes, dst_to_src.data());

      auto& ni = result.pruned_node_indices[i];
      ni.resize(collide_res.num_dst_internodes);
      extract_node_indices(collide_res.dst_internodes, collide_res.num_dst_internodes, ni.data());
    }

    if (max_num_found_holes > 0) {
      assert(max_num_found_holes_ind < int(candidate_wall_holes.size()));
      auto& src_holes = candidate_wall_holes[max_num_found_holes_ind];
      result.holes.resize(max_num_found_holes);
      std::copy(src_holes.data(), src_holes.data() + max_num_found_holes, result.holes.data());
    }
  }

  //  Use holes from the candidate that produced the largest number of holes, if there was one.
  //  For the remaining candidates, conservatively prune all axes intersecting the target bounds.
  for (int i = 0; i < int(result.pruned_dst_to_src.size()); i++) {
    if (try_compute_holes && i == max_num_found_holes_ind) {
      continue;
    }

    auto& src_nodes = candidate_roots[i]->nodes;
    const int num_src_nodes = candidate_roots[i]->curr_num_nodes;

    Temporary<bool, 2048> store_accept;
    auto* accept = store_accept.require(num_src_nodes);
    reject_intersecting(src_nodes.data(), num_src_nodes, params.wall_bounds, accept);

    auto& pruned_dst_to_src = result.pruned_dst_to_src[i];
    auto& pruned_node_indices = result.pruned_node_indices[i];
    pruned_dst_to_src.resize(num_src_nodes);
    pruned_node_indices.resize(num_src_nodes);
    const int num_kept = prune_rejected_axes(
      src_nodes.data(), accept, num_src_nodes,
      pruned_node_indices.data(), pruned_dst_to_src.data());
    pruned_dst_to_src.resize(num_kept);
    pruned_node_indices.resize(num_kept);
  }

  return result;
}

std::vector<tree::RootsInstanceHandle>
arch::start_pruning_collided(ComputeWallHolesAroundRootsResult&& result, RootsSystem* roots_system) {
  assert(result.pruned_dst_to_src.size() == result.pruned_node_indices.size() &&
         result.pruned_dst_to_src.size() == result.pruned_instances.size());
  for (int i = 0; i < int(result.pruned_dst_to_src.size()); i++) {
    tree::start_pruning_roots(
      roots_system,
      result.pruned_instances[i],
      std::move(result.pruned_dst_to_src[i]),
      std::move(result.pruned_node_indices[i]));
  }
  return result.pruned_instances;
}

GROVE_NAMESPACE_END
