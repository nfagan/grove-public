#pragma once

#include "components.hpp"
#include "grove/common/Stopwatch.hpp"

namespace grove::tree {

enum class GrowthState {
  Idle,
  ConsumeAttractionPoints,
  SenseEnvironment,
  ApplyEnvironmentInput,
  DetermineBudFate,
  SetRenderPosition,
};

struct GrowthCycleContext {
  std::vector<TreeNodeStore*> trees;
  std::vector<const SpawnInternodeParams*> spawn_internode_params;
  std::vector<const DistributeBudQParams*> distribute_bud_q_params;
  GrowthState state{GrowthState::Idle};
  Stopwatch stopwatch;
  int active_tree{};
  int active_bud{};
  EnvironmentInputs environment_input;
  AttractionPoints* attraction_points{};
  SenseContext sense_context;
};

struct GrowthCycleParams {
  double time_limit_seconds{1.0e-3};
};

void initialize_growth_cycle(GrowthCycleContext& context,
                             AttractionPoints* attraction_points,
                             std::vector<TreeNodeStore*>&& trees,
                             std::vector<const SpawnInternodeParams*>&& spawn_params,
                             std::vector<const DistributeBudQParams*>&& bud_q_params);

void growth_cycle(GrowthCycleContext& context, const GrowthCycleParams& params);

}