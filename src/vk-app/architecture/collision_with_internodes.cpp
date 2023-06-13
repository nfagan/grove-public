#include "collision_with_internodes.hpp"
#include "../procedural_tree/render.hpp"
#include "../procedural_tree/utility.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Temporary.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace arch;

using BoundsIDSet = InternodeBoundsIntersectResult::BoundsIDSet;
using LeafBoundsIDMap = InternodeBoundsIntersectResult::LeafBoundsIDMap;
using BoundsIDVec = std::vector<bounds::ElementID>;

template <typename Container>
bool can_prune_candidates(const tree::TreeSystem* sys, const Container& candidates) {
  for (tree::TreeInstanceHandle candidate : candidates) {
    if (!tree::can_start_pruning(sys, candidate)) {
      return false;
    }
  }
  return true;
}

std::vector<tree::TreeInstanceHandle> lookup_tree_instances(const tree::TreeSystem* sys,
                                                            const BoundsIDSet& from_bounds_ids) {
  std::vector<tree::TreeInstanceHandle> result;
  for (bounds::ElementID parent_id : from_bounds_ids) {
    if (auto handle = tree::lookup_instance_by_bounds_element_id(sys, parent_id)) {
      result.push_back(handle.value());
    }
  }
  return result;
}

std::vector<tree::TreeInstanceHandle> lookup_tree_instances(const tree::TreeSystem* sys,
                                                            const LeafBoundsIDMap& leaf_bounds_ids) {
  std::vector<tree::TreeInstanceHandle> result;
  for (auto& [parent_id, _] : leaf_bounds_ids) {
    if (auto handle = tree::lookup_instance_by_bounds_element_id(sys, parent_id)) {
      result.push_back(handle.value());
    }
  }
  return result;
}

void reject_intersecting(const tree::Internodes& src_inodes, const OBB3f& bounds, bool* accept) {
  int ni{};
  for (auto& node : src_inodes) {
    auto node_obb = tree::internode_obb(node);
    if (obb_obb_intersect(bounds, node_obb)) {
      accept[ni] = false;
    } else {
      accept[ni] = true;
    }
    ni++;
  }
}

} //  anon

InternodeBoundsIntersectResult
arch::internode_bounds_intersect(const bounds::Accel* accel, const OBB3f& query_bounds,
                                 const tree::TreeSystem* tree_system,
                                 const Optional<bounds::ElementID>& allow_element) {
  InternodeBoundsIntersectResult result{};

  std::vector<const bounds::Element*> isect;
  accel->intersects(bounds::make_query_element(query_bounds), isect);
  result.any_hit = !isect.empty();

  const auto tree_bounds_tag = tree::get_bounds_tree_element_tag(tree_system);
  const auto leaf_bounds_tag = tree::get_bounds_leaf_element_tag(tree_system);

  for (const bounds::Element* el : isect) {
    if (el->tag == tree_bounds_tag.id) {
      result.parent_ids_from_internodes.insert(bounds::ElementID{el->parent_id});

    } else if (el->tag == leaf_bounds_tag.id) {
      bounds::ElementID parent_id{el->parent_id};
      bounds::ElementID el_id{el->id};
      if (result.leaf_element_ids_by_parent_id.count(parent_id) == 0) {
        result.leaf_element_ids_by_parent_id[parent_id] = BoundsIDSet{el_id};
      } else {
        result.leaf_element_ids_by_parent_id.at(parent_id).insert(el_id);
      }
    } else {
      if (!allow_element || allow_element.value().id != el->id) {
        result.any_hit_besides_trees_or_leaves = true;
      }
    }
  }

  return result;
}

bool arch::can_prune_all_candidates(const tree::TreeSystem* sys,
                                    const InternodeBoundsIntersectResult& isect_res) {
  auto inst_handles0 = lookup_tree_instances(sys, isect_res.parent_ids_from_internodes);
  auto inst_handles1 = lookup_tree_instances(sys, isect_res.leaf_element_ids_by_parent_id);
  return can_prune_candidates(sys, inst_handles0) && can_prune_candidates(sys, inst_handles1);
}

std::vector<tree::TreeInstanceHandle>
arch::start_pruning_collided(std::vector<InternodesPendingPrune>&& pending_prune,
                             ReevaluateLeafBoundsMap&& reevaluate_leaf_bounds,
                             tree::TreeSystem* tree_sys) {
  std::vector<tree::TreeInstanceHandle> all_pending;

  for (auto& pend : pending_prune) {
    tree::TreeSystem::PruningInternodes pruning_inodes;
    pruning_inodes.dst_to_src = std::move(pend.dst_to_src);
    pruning_inodes.internodes = std::move(pend.dst_internodes);

    tree::TreeSystem::PruningData pruning_data;
    pruning_data.internodes = std::move(pruning_inodes);

    auto leaf_it = reevaluate_leaf_bounds.find(pend.handle);
    if (leaf_it != reevaluate_leaf_bounds.end()) {
      pruning_data.leaves.remove_bounds = std::move(leaf_it->second);
      reevaluate_leaf_bounds.erase(leaf_it);
    }

    tree::start_pruning(tree_sys, pend.handle, std::move(pruning_data));
    all_pending.push_back(pend.handle);
  }

  //  Remaining
  for (auto& [handle, element_ids] : reevaluate_leaf_bounds) {
    tree::TreeSystem::PruningData pruning_data;
    pruning_data.leaves.remove_bounds = std::move(element_ids);
    tree::start_pruning(tree_sys, handle, std::move(pruning_data));
    all_pending.push_back(handle);
  }

  return all_pending;
}

ComputeWallHolesAroundInternodesResult
arch::compute_wall_holes_around_internodes(const InternodeBoundsIntersectResult& isect_res,
                                           const ComputeWallHolesAroundInternodesParams& params) {
  ComputeWallHolesAroundInternodesResult result{};

  if (!isect_res.any_hit) {
    return result;
  }

  auto& leaf_ids = isect_res.leaf_element_ids_by_parent_id;
  auto& candidate_tree_ids = isect_res.parent_ids_from_internodes;
  for (auto& [leaf_parent_id, element_ids] : leaf_ids) {
    auto tree_handle = tree::lookup_instance_by_bounds_element_id(
      params.tree_system, leaf_parent_id);
    if (tree_handle) {
      result.reevaluate_leaf_bounds[tree_handle.value()] = BoundsIDVec{
        element_ids.begin(), element_ids.end()};
    }
  }

  if (candidate_tree_ids.empty()) {
    return result;
  }

  std::vector<const tree::Internodes*> candidate_internodes;
  std::vector<tree::TreeInstanceHandle> candidate_handles;

  for (bounds::ElementID candidate_id : candidate_tree_ids) {
    auto tree_handle = tree::lookup_instance_by_bounds_element_id(
      params.tree_system, candidate_id);
    if (tree_handle) {
      const auto read_inst = tree::read_tree(params.tree_system, tree_handle.value());
      if (read_inst.nodes) {
        candidate_internodes.push_back(&read_inst.nodes->internodes);
        candidate_handles.push_back(tree_handle.value());
      }
    }
  }

  if (candidate_internodes.empty()) {
    return result;
  }

  const bool try_compute_holes = params.collide_through_hole_params != nullptr;
  int max_num_found_holes_ind{};
  int max_num_found_holes{-1};

  if (try_compute_holes) {
    constexpr int max_num_wall_holes = 4;
    std::vector<std::vector<arch::WallHole>> candidate_wall_holes;
    std::vector<tree::Internodes> pruned_internodes;
    std::vector<std::vector<int>> pruned_to_src;

    for (int i = 0; i < int(candidate_internodes.size()); i++) {
      const tree::Internodes* src_nodes = candidate_internodes[i];

      auto& holes = candidate_wall_holes.emplace_back();
      holes.resize(max_num_wall_holes);

      TreeNodeCollisionWithWallParams collide_params{};
      collide_params.collision_context = params.collision_context;
      collide_params.collide_through_hole_params = params.collide_through_hole_params;
      collide_params.wall_bounds = params.wall_bounds;
      collide_params.src_internodes = src_nodes->data();
      collide_params.num_src_internodes = int(src_nodes->size());
      collide_params.accepted_holes = holes.data();
      collide_params.max_num_accepted_holes = max_num_wall_holes;
      auto collide_res = compute_collision_with_wall(collide_params);

      holes.resize(collide_res.num_accepted_bounds_components);
      if (collide_res.num_accepted_bounds_components > max_num_found_holes) {
        max_num_found_holes = collide_res.num_accepted_bounds_components;
        max_num_found_holes_ind = i;
      }

      auto& dst_inodes = pruned_internodes.emplace_back();
      dst_inodes.resize(collide_res.num_dst_internodes);
      std::copy(
        collide_res.dst_internodes,
        collide_res.dst_internodes + collide_res.num_dst_internodes,
        dst_inodes.data());

      auto& dst_to_src = pruned_to_src.emplace_back();
      dst_to_src.resize(collide_res.num_dst_internodes);
      std::copy(
        collide_res.dst_to_src,
        collide_res.dst_to_src + collide_res.num_dst_internodes,
        dst_to_src.data());
    }

    auto& prune_through_hole = result.pending_prune.emplace_back();
    prune_through_hole.handle = candidate_handles[max_num_found_holes_ind];
    prune_through_hole.dst_internodes = std::move(pruned_internodes[max_num_found_holes_ind]);
    prune_through_hole.dst_to_src = std::move(pruned_to_src[max_num_found_holes_ind]);
    result.holes = std::move(candidate_wall_holes[max_num_found_holes_ind]);
  }

  //  For candidates other than `max_num_found_holes_ind` (or for all candidates, in the case that
  //  `try_compute_holes` is false), simply prune all axes intersecting the wall bounds.
  for (int i = 0; i < int(candidate_internodes.size()); i++) {
    if (try_compute_holes && i == max_num_found_holes_ind) {
      continue;
    }

    const tree::Internodes* src_inodes = candidate_internodes[i];

    Temporary<bool, 1024> store_accept;
    auto* accept = store_accept.require(int(src_inodes->size()));
    reject_intersecting(*src_inodes, params.wall_bounds, accept);

    auto dst_inodes = *src_inodes;
    std::vector<int> dst_to_src(src_inodes->size());
    dst_inodes.resize(tree::prune_rejected_axes(
      src_inodes->data(), accept, int(src_inodes->size()),
      dst_inodes.data(), dst_to_src.data()));
    dst_to_src.resize(dst_inodes.size());

    auto& prune_through_hole = result.pending_prune.emplace_back();
    prune_through_hole.handle = candidate_handles[i];
    prune_through_hole.dst_internodes = std::move(dst_inodes);
    prune_through_hole.dst_to_src = std::move(dst_to_src);
  }

  return result;
}

GROVE_NAMESPACE_END
