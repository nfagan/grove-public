#pragma once

#include "components.hpp"

namespace grove::tree {

//  Write at most `max_num_points` to `dst`, return num actually written.
using MakeAttractionPoints = std::function<int(Vec3f* dst, int max_num_points)>;

struct GrowableTree {
  TreeNodeStore* nodes;
  const SpawnInternodeParams* spawn_params;
  const DistributeBudQParams* bud_q_params;
  const MakeAttractionPoints* make_attraction_points;
  bool finished_growing;
  int max_num_internodes;
  int last_num_internodes;
};

struct GrowthContext {
  ArrayView<GrowableTree> trees;
  ArrayView<Vec3f> attraction_points_buffer;
  EnvironmentInputs* environment_input;
  AttractionPoints* attraction_points;
  SenseContext* sense_context;
};

struct GrowthResult {
  double elapsed_time;
};

GrowthResult grow(GrowthContext* context);
GrowableTree make_growable_tree(TreeNodeStore* nodes,
                                const SpawnInternodeParams* spawn_params,
                                const DistributeBudQParams* bud_q_params,
                                const MakeAttractionPoints* make_attraction_points,
                                int max_num_internodes);

}