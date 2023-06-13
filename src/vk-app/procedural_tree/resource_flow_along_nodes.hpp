#pragma once

#include "grove/math/Vec3.hpp"
#include "grove/math/Mat3.hpp"
#include "grove/common/identifier.hpp"

namespace grove::tree {

struct ResourceSpiralAroundNodesSystem;
struct TreeInstanceHandle;
struct RootsInstanceHandle;
struct TreeSystem;
struct RootsSystem;

struct ResourceSpiralCylinderNode {
  Vec3f position;
  float radius;
};

struct ResourceSpiralAroundNodesSystemStats {
  int num_instances;
  int num_free_instances;
  float current_global_vel0;
  float current_global_theta0;
  float current_global_vel1;
  float current_global_theta1;
};

struct ResourceSpiralAroundNodesHandle {
  GROVE_INTEGER_IDENTIFIER_IS_VALID(index)
  int index;
};

struct SpiralAroundNodesQuadVertexTransform {
  Vec3f p;
  Vec3f frame_x;
};

struct SpiralAroundNodesUpdateContext {
  static constexpr int max_num_points_per_segment = 16;

  bool active;
  float t;

  int num_points_per_segment;
  SpiralAroundNodesQuadVertexTransform points[max_num_points_per_segment * 2];
  int point_segment0_end;
  int point_segment1_end;

  int next_ni;
  Vec3f next_p;
  Vec3<uint8_t> color;
  uint8_t render_pipeline_index;
  bool burrowing;

  float distance_to_camera;
  float velocity_scale;
  float scale;
  float fade_frac;
  bool fadeout;
};

struct ResourceSpiralAroundNodesUpdateInfo {
  const TreeSystem* tree_sys;
  const RootsSystem* roots_sys;
  double real_dt;
  const Vec3f& camera_position;
};

struct CreateResourceSpiralParams {
  uint8_t global_param_set_index;
  float theta_offset;
  float scale;
  Vec3<uint8_t> linear_color;
  uint8_t render_pipeline_index;
  bool burrows_into_target;
  bool non_fixed_parent_origin;
};

ResourceSpiralAroundNodesSystem* get_global_resource_spiral_around_nodes_system();
void terminate_resource_spiral_around_nodes_system(ResourceSpiralAroundNodesSystem* sys);

void update_resource_spiral_around_nodes(
  ResourceSpiralAroundNodesSystem* sys, const ResourceSpiralAroundNodesUpdateInfo& info);
void set_global_velocity_scale(ResourceSpiralAroundNodesSystem* sys, uint8_t param_set, float v);
void set_global_theta(ResourceSpiralAroundNodesSystem* sys, uint8_t param_set, float th);

ResourceSpiralAroundNodesHandle create_resource_spiral_around_tree(
  ResourceSpiralAroundNodesSystem* sys, const TreeInstanceHandle& tree,
  const CreateResourceSpiralParams& params);

ResourceSpiralAroundNodesHandle create_resource_spiral_around_roots(
  ResourceSpiralAroundNodesSystem* sys, const RootsInstanceHandle& roots,
  const CreateResourceSpiralParams& params);

ResourceSpiralAroundNodesHandle create_resource_spiral_around_line_of_cylinders(
  ResourceSpiralAroundNodesSystem* sys, const ResourceSpiralCylinderNode* nodes, int num_nodes,
  const CreateResourceSpiralParams& params);

void set_resource_spiral_scale(
  ResourceSpiralAroundNodesSystem* sys, ResourceSpiralAroundNodesHandle handle, float s);

void set_resource_spiral_velocity_scale(
  ResourceSpiralAroundNodesSystem* sys, ResourceSpiralAroundNodesHandle handle, float s);

void destroy_resource_spiral(
  ResourceSpiralAroundNodesSystem* sys, ResourceSpiralAroundNodesHandle handle);

const SpiralAroundNodesUpdateContext*
read_contexts(const ResourceSpiralAroundNodesSystem* sys, int* num_contexts);

ResourceSpiralAroundNodesSystemStats get_stats(const ResourceSpiralAroundNodesSystem* sys);

}