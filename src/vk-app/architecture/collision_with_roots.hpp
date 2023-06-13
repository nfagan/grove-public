#pragma once

#include "../procedural_tree/roots_system.hpp"
#include "../procedural_tree/collide_with_object.hpp"
#include "wall_holes_around_tree_nodes.hpp"

namespace grove::arch {

struct RootBoundsIntersectResult {
  bool any_hit_besides_tree_or_roots{};
  bool any_hit_roots{};
  std::vector<bounds::RadiusLimiterAggregateID> hit_root_aggregate_ids; //  will contain unique ids
};

struct ComputeWallHolesAroundRootsParams {
  const RootBoundsIntersectResult* intersect_result;
  OBB3f wall_bounds;
  const tree::RootsSystem* roots_system;
  tree::TreeNodeCollisionWithObjectContext* collision_context;
  const TreeNodeCollideThroughHoleParams* collide_through_hole_params;
};

struct ComputeWallHolesAroundRootsResult {
  std::vector<tree::RootsInstanceHandle> pruned_instances;
  std::vector<std::vector<int>> pruned_dst_to_src;
  std::vector<std::vector<tree::TreeRootNodeIndices>> pruned_node_indices;
  std::vector<arch::WallHole> holes;
};

RootBoundsIntersectResult root_bounds_intersect(
  const bounds::RadiusLimiter* lim, const OBB3f& bounds,
  bounds::RadiusLimiterElementTag roots_tag,
  bounds::RadiusLimiterElementTag tree_tag,
  const Optional<bounds::RadiusLimiterAggregateID>& allow_isect);

bool can_prune_all_candidates(const tree::RootsSystem* sys, const RootBoundsIntersectResult& result);

ComputeWallHolesAroundRootsResult
compute_wall_holes_around_roots(const ComputeWallHolesAroundRootsParams& params);

std::vector<tree::RootsInstanceHandle>
start_pruning_collided(ComputeWallHolesAroundRootsResult&& result, tree::RootsSystem* roots_system);

}