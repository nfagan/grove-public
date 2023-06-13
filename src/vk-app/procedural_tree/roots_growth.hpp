#pragma once

#include "roots_components.hpp"

namespace grove::tree {

struct AssignRootsDiameterParams {
  float leaf_diameter;
  float diameter_power;
};

struct GrowRootsParams {
  double real_dt;
  float growth_rate;
  Vec3f attractor_point;
  float attractor_point_scale;
  double p_spawn_lateral;
  float node_length_scale;
  float min_axis_length_spawn_lateral;
  bool disable_node_creation;
};

struct GrowRootsResult {
  bool finished;
  int num_nodes_added;
  int num_new_branches;
  int next_growing_ni_begin;
};

struct RecedeRootsResult {
  bool finished;
};

struct PruneRootsResult {
  bool finished;
};

GrowRootsResult grow_roots(TreeRoots* roots, bounds::RadiusLimiter* lim,
                           bounds::RadiusLimiterElementHandle* elements,
                           bounds::RadiusLimiterElementTag roots_tag,
                           TreeRootsGrowthContext& growth_context,
                           const GrowRootsParams& grow_params,
                           const AssignRootsDiameterParams& diameter_params);

RecedeRootsResult recede_roots(TreeRoots* roots, bounds::RadiusLimiter* lim,
                               bounds::RadiusLimiterElementHandle* bounds_elements,
                               TreeRootsRecedeContext& recede_context, const GrowRootsParams& params);

PruneRootsResult prune_roots(TreeRoots* roots, bounds::RadiusLimiter* lim,
                             bounds::RadiusLimiterElementHandle* bounds_elements,
                             TreeRootsRecedeContext& recede_context, const GrowRootsParams& params);

void assign_diameter(TreeRootNode* root, const AssignRootsDiameterParams& params);

void init_roots_recede_context(TreeRootsRecedeContext* context, TreeRootNode* nodes, int num_nodes,
                               const TreeRootsSkipReceding* skip = nullptr);

}