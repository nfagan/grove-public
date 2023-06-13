#pragma once

#include "../bounds/common.hpp"
#include "geometry.hpp"
#include "../procedural_tree/collide_with_object.hpp"
#include "../procedural_tree/tree_system.hpp"
#include "wall_holes_around_tree_nodes.hpp"

namespace grove::arch {

struct InternodesPendingPrune {
  tree::TreeInstanceHandle handle{};
  tree::Internodes dst_internodes;
  std::vector<int> dst_to_src;
};

using ReevaluateLeafBoundsMap = std::unordered_map<tree::TreeInstanceHandle,
  std::vector<bounds::ElementID>, tree::TreeInstanceHandle::Hash>;

struct ComputeWallHolesAroundInternodesResult {
  std::vector<arch::WallHole> holes;
  std::vector<InternodesPendingPrune> pending_prune;
  ReevaluateLeafBoundsMap reevaluate_leaf_bounds;
};

struct ComputeWallHolesAroundInternodesParams {
  OBB3f wall_bounds;
  const tree::TreeSystem* tree_system;
  tree::TreeNodeCollisionWithObjectContext* collision_context;
  const TreeNodeCollideThroughHoleParams* collide_through_hole_params;
};

struct InternodeBoundsIntersectResult {
  using BoundsIDSet = std::unordered_set<bounds::ElementID, bounds::ElementID::Hash>;
  using LeafBoundsIDMap = std::unordered_map<bounds::ElementID, BoundsIDSet, bounds::ElementID::Hash>;

  bool any_hit;
  bool any_hit_besides_trees_or_leaves;
  BoundsIDSet parent_ids_from_internodes;
  LeafBoundsIDMap leaf_element_ids_by_parent_id;
};

InternodeBoundsIntersectResult internode_bounds_intersect(const bounds::Accel* accel,
                                                          const OBB3f& query_bounds,
                                                          const tree::TreeSystem* tree_system,
                                                          const Optional<bounds::ElementID>& allow_element);

bool can_prune_all_candidates(const tree::TreeSystem* sys,
                              const InternodeBoundsIntersectResult& isect_res);

std::vector<tree::TreeInstanceHandle>
start_pruning_collided(std::vector<InternodesPendingPrune>&& pending_prune,
                       ReevaluateLeafBoundsMap&& reevaluate_leaf_bounds,
                       tree::TreeSystem* tree_sys);

ComputeWallHolesAroundInternodesResult
compute_wall_holes_around_internodes(const InternodeBoundsIntersectResult& isect_res,
                                     const ComputeWallHolesAroundInternodesParams& params);

}