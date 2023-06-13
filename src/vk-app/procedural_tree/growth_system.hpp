#pragma once

#include "components.hpp"
#include "growth.hpp"
#include "grove/common/Future.hpp"
#include <atomic>
#include <future>

namespace grove::tree {

struct GrowthContextHandle {
  GROVE_INTEGER_IDENTIFIER_EQUALITY(GrowthContextHandle, id)
  GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
  uint32_t id;
};

struct PrepareToGrowParams {
  GrowthContextHandle context;
  TreeNodeStore nodes;
  SpawnInternodeParams spawn_params;
  DistributeBudQParams bud_q_params;
  MakeAttractionPoints make_attraction_points;
  int max_num_internodes;
};

struct CreateGrowthContextParams {
  int max_num_attraction_points_per_tree;
  float initial_attraction_point_span_size;
  float max_attraction_point_span_size_split;
};

//  @TODO: Remove old growth system, rename.
struct GrowthSystem2 {
public:
  struct Events {
    bool just_finished_growing;
    bool just_finished_clearing_attraction_points;
  };

  enum class AsyncState {
    Idle = 0,
    Growing,
    ClearingAttractionPoints
  };

  struct NodeGrowthResult {
    TreeNodeStore nodes;
    GrowthContextHandle context_handle;
    SpawnInternodeParams spawn_params;
    DistributeBudQParams bud_q_params;
    MakeAttractionPoints make_attraction_points;
  };

  using FutureGrowthResult = std::shared_ptr<Future<NodeGrowthResult>>;

  struct Instance {
    GrowthContextHandle context_handle;
    TreeNodeStore nodes;
    SpawnInternodeParams spawn_params;
    DistributeBudQParams bud_q_params;
    MakeAttractionPoints make_attraction_points;
    int max_num_internodes;
    bool growing;
    FutureGrowthResult result;
  };

  struct GrowthContext {
    uint32_t id;
    EnvironmentInputs environment_input;
    AttractionPoints attraction_points;
    SenseContext sense_context;
    std::vector<GrowableTree> growable_trees;
    std::unique_ptr<Vec3f[]> attraction_points_buffer;
    int attraction_points_buffer_size;
    bool awaiting_growth;
    AsyncState async_state;
    std::vector<std::unique_ptr<Instance>> growing_instances;
    std::vector<tree::TreeID> pending_clear_attraction_points;
    std::atomic<bool> async_finished;
    std::future<void> async_future;
    GrowthResult growth_result;
    Events events;
  };

  struct ReadGrowthContext {
    const AttractionPoints* attraction_points;
    const GrowthResult* growth_result;
    Events events;
  };

public:
  std::vector<std::unique_ptr<Instance>> instances;
  std::vector<std::unique_ptr<GrowthContext>> growth_contexts;
  uint32_t next_growth_context_id{1};
};

GrowthContextHandle create_growth_context(GrowthSystem2* sys,
                                          const CreateGrowthContextParams& params);
GrowthSystem2::ReadGrowthContext read_growth_context(const GrowthSystem2* sys,
                                                     GrowthContextHandle handle);

[[nodiscard]] GrowthSystem2::FutureGrowthResult prepare_to_grow(GrowthSystem2* sys,
                                                                PrepareToGrowParams&& params);
void push_pending_attraction_points_clear(GrowthSystem2* sys, GrowthContextHandle handle,
                                          tree::TreeID id);
bool can_grow(const GrowthSystem2* sys, GrowthContextHandle context);
void grow(GrowthSystem2* sys, GrowthContextHandle context);
void update(GrowthSystem2* sys);

}