#pragma once

#include "grove/math/Vec3.hpp"

namespace grove {
class Terrain;
}

namespace grove::tree {
struct RootsSystem;
struct RenderRootsSystem;
struct ResourceSpiralAroundNodesSystem;
}

namespace grove {

struct TreeRootsComponent;

struct TreeRootsComponentInitInfo {
  tree::RootsSystem* roots_system;
  tree::RenderRootsSystem* render_roots_system;
};

struct TreeRootsComponentUpdateInfo {
  tree::RootsSystem* roots_system;
  tree::RenderRootsSystem* render_roots_system;
  tree::ResourceSpiralAroundNodesSystem* resource_spiral_system;
  const Vec3f* newly_created_tree_origins;
  int num_newly_created_trees;
  bool can_trigger_recede;
  const Terrain& terrain;
};

struct TreeRootsComponentCreateRootsParams {
  Vec3f position;
  Vec3f direction;
  bool use_terrain_height;
  int n;
  float r;
};

TreeRootsComponent* get_global_tree_roots_component();
void init_tree_roots_component(TreeRootsComponent* component, const TreeRootsComponentInitInfo& info);
void update_tree_roots_component(TreeRootsComponent* component, const TreeRootsComponentUpdateInfo& info);
void tree_roots_component_defer_create_roots(TreeRootsComponent* component,
                                             const TreeRootsComponentCreateRootsParams& params);
void tree_roots_component_simple_create_roots(
  TreeRootsComponent* component, const Vec3f& p, int n, bool up, bool use_terrain_height = true);

}