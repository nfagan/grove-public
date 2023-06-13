#pragma once

#include "grove/math/OBB3.hpp"

namespace grove::tree {
struct Internode;
struct TreeNodeCollisionWithObjectContext;
struct TreeNodeCollisionWithObjectResult;
}

namespace grove::arch {

struct WallHole;

struct TreeNodeCollideThroughHoleParams {
  int forward_dim{2};
  float min_collide_node_diam{0.025f};
  float projected_aabb_scale{1.5f};
  float hole_curl{0.2f};
  bool prune_initially_rejected{true};
  bool reject_all_holes{};
};

struct TreeNodeCollisionWithWallParams {
  tree::TreeNodeCollisionWithObjectContext* collision_context;
  const TreeNodeCollideThroughHoleParams* collide_through_hole_params;
  OBB3f wall_bounds;
  const tree::Internode* src_internodes;
  int num_src_internodes;
  arch::WallHole* accepted_holes;
  int max_num_accepted_holes;
};

tree::TreeNodeCollisionWithObjectResult
compute_collision_with_wall(const TreeNodeCollisionWithWallParams& params);

}