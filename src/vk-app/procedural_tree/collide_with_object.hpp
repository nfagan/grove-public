#pragma once

#include "components.hpp"

namespace grove::tree {

struct TreeNodeCollisionEntry {
  int internode_index;
};

struct ProjectCollidingBoundsEntry {
  bool accepted;
  Bounds2f bounds;
};

struct TreeNodeCollisionWithObjectResult {
  const int* dst_to_src;
  const tree::Internode* dst_internodes;
  int num_dst_internodes;
  int num_accepted_bounds_components;
  const OBB3f* collided_bounds;
  int num_collided_bounds;
};

struct AcceptCollisionComponentBoundsParams {
  const Bounds2f* projected_component_bounds;
  const int* unique_component_ids;
  int num_components;
  int* accept_component_ids;
  int* num_accepted;
};

using AcceptCollisionComponentBounds = std::function<void(const AcceptCollisionComponentBoundsParams&)>;

struct TreeNodeCollisionWithObjectContext {
  int num_reserved_instances{};
  std::unique_ptr<TreeNodeCollisionEntry[]> collision_entries;
  std::unique_ptr<ProjectCollidingBoundsEntry[]> project_bounds_entries;
  std::unique_ptr<OBB3f[]> internode_bounds;
  std::unique_ptr<Bounds2f[]> projected_bounds;
  std::unique_ptr<tree::Internode[]> aux_src_internodes;
  std::unique_ptr<int[]> aux_dst_to_src;
  std::unique_ptr<int[]> bounds_component_ids;
  std::unique_ptr<int[]> unique_bounds_component_ids;
  std::unique_ptr<int[]> accept_bounds_component_ids;
  std::unique_ptr<bool[]> accept_internode;
  std::unique_ptr<tree::Internode[]> dst_internodes;
  std::unique_ptr<int[]> dst_to_src;
};

struct TreeNodeCollisionWithObjectParams {
  OBB3f object_bounds;
  const Internode* src_internodes;
  int num_src_internodes;
  float min_colliding_node_diameter;
  int project_forward_dim;
  float projected_aabb_scale;
  bool prune_initially_rejected;
  AcceptCollisionComponentBounds accept_collision_component_bounds;
};

void reserve(TreeNodeCollisionWithObjectContext* ctx, int num_internodes);

TreeNodeCollisionWithObjectResult
compute_collision_with_object(TreeNodeCollisionWithObjectContext* ctx,
                              const TreeNodeCollisionWithObjectParams& params);

}