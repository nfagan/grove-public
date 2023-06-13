#pragma once

#include "components.hpp"
#include "growth_system.hpp"
#include "accel_insert.hpp"

#define GROVE_INCLUDE_TREE_INTERNODES_IN_RADIUS_LIMITER (1)

#if GROVE_INCLUDE_TREE_INTERNODES_IN_RADIUS_LIMITER
#include "radius_limiter.hpp"
#endif

#include <unordered_set>

namespace grove::tree {

struct TreeInstanceHandle {
  GROVE_INTEGER_IDENTIFIER_STD_HASH(Hash, TreeInstanceHandle, id)
  GROVE_INTEGER_IDENTIFIER_EQUALITY(TreeInstanceHandle, id)
  GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
  uint32_t id;
};

enum class TreeSystemLeafBoundsDistributionStrategy : uint8_t {
  Original = 0,
  AxisAlignedOutwardsFromNodes
};

struct CreateTreeParams {
  Vec3f origin;
  SpawnInternodeParams spawn_params;
  DistributeBudQParams bud_q_params;
  MakeAttractionPoints make_attraction_points;
  Optional<bounds::AccelInstanceHandle> insert_into_accel;
  Vec3f leaf_internode_bounds_scale{1.0f};
  Vec3f leaf_internode_bounds_offset{0.0f};
  TreeSystemLeafBoundsDistributionStrategy leaf_bounds_distribution_strategy{};
};

struct TreeSystem {
  struct PrepareToGrowParams {
    GrowthContextHandle context;
    int max_num_internodes;
  };

  struct Events {
    bool node_structure_modified;
    bool node_render_position_modified;
    bool just_started_render_growing;
    bool just_started_awaiting_finish_growth_signal;
    bool just_started_awaiting_finish_render_growth_signal;
    bool just_finished_render_death;
    bool just_started_pruning;
    bool just_started_awaiting_finish_pruning_signal;
    bool just_finished_pruning;
  };

  enum class ModifyingState : uint8_t {
    Idle = 0,
    Growing,
    RenderGrowing,
    Pruning,
    RenderDying
  };

  enum class ModifyingPhase : uint8_t {
    Idle = 0,
    GeneratingNodeStructure,
    NodeAccelInsertingAndPruning,
    LeafAccelInserting,
    AwaitingFinishGrowingSignal,
    FinishedGrowingSignalReceived,

    RenderGrowing,
    AwaitingFinishRenderGrowingSignal,
    FinishedRenderGrowingSignalReceived,

    AwaitingFinishPruningLeavesSignal,
    FinishedPruningLeavesSignalReceived,
    PruningInternodes,
    AwaitingFinishPruningSignal,
    FinishedPruningSignalReceived,
  };

  struct GrowthState {
    ModifyingState modifying;
    ModifyingPhase phase;
    bool pending_growth;
    bool pending_render_growth;
    bool pending_prune;
    bool pending_render_death;
  };

  struct InsertedAttractionPoints {
    GrowthContextHandle context;
    TreeID id;
  };

  struct Leaves {
    std::vector<bounds::ElementID> inserted_bounds;
    Vec3f internode_bounds_scale{1.0f};
    Vec3f internode_bounds_offset{};
    TreeSystemLeafBoundsDistributionStrategy bounds_distribution_strategy{};
  };

  struct PruningLeaves {
    std::vector<bounds::ElementID> remove_bounds;
  };

  struct PruningInternodes {
    tree::Internodes internodes;
    std::vector<int> dst_to_src;
  };

  struct PruningData {
    PruningLeaves leaves;
    Optional<PruningInternodes> internodes;
  };

  struct Instance {
    TreeNodeStore nodes;
    Leaves leaves;
    SpawnInternodeParams spawn_params;
    DistributeBudQParams bud_q_params;
    MakeAttractionPoints make_attraction_points;
    GrowthSystem2::FutureGrowthResult future_growth_result;
    PrepareToGrowParams prepare_to_grow_params;
    GrowthState growth_state;
    Events events;
    float axis_growth_incr;
    Bounds3f src_aabb;
    std::unique_ptr<RenderAxisGrowthContext> axis_growth_context;
    std::unique_ptr<RenderAxisDeathContext> axis_death_context;
    Optional<bounds::AccelInstanceHandle> insert_into_accel;
    bounds::ElementID bounds_element_id;
    std::vector<bounds::ElementID> inserted_internode_bounds;
    FutureInsertAndPruneResult future_insert_and_prune_result;
    std::unique_ptr<PruningData> pruning_data;
#if GROVE_INCLUDE_TREE_INTERNODES_IN_RADIUS_LIMITER
    std::vector<bounds::RadiusLimiterElementHandle> inserted_radius_limiter_elements;
#endif
  };

  struct ReadInstance {
    const TreeNodeStore* nodes;
    const Bounds3f* src_aabb;
    const Leaves* leaves;
    GrowthState growth_state;
    Events events;
    bounds::ElementID bounds_element_id;
  };

  struct UpdateInfo {
#if GROVE_INCLUDE_TREE_INTERNODES_IN_RADIUS_LIMITER
    bounds::RadiusLimiter* radius_limiter;
    bounds::RadiusLimiterElementTag roots_tag;
#endif
    GrowthSystem2* growth_system;
    AccelInsertAndPrune* accel_insert_and_prune;
    bounds::BoundsSystem* bounds_system;
    double real_dt;
  };

  using DeletedInstances = std::unordered_set<TreeInstanceHandle, TreeInstanceHandle::Hash>;
  struct UpdateResult {
    const DeletedInstances* just_deleted;
  };

  struct Stats {
    int num_instances;
    int num_axis_growth_contexts;
    int num_axis_death_contexts;
    int num_pending_deletion;
    int num_inserted_attraction_points;
    int max_num_instances_generated_node_structure_in_one_frame;
    double max_time_spent_generating_node_structure_s;
    double max_time_spent_state_growing_s;
    double max_time_spent_pruning_against_radius_limiter_s;
  };

public:
  std::unordered_map<uint32_t, Instance> instances;
  uint32_t next_instance_id{1};

  std::vector<std::unique_ptr<RenderAxisGrowthContext>> axis_growth_contexts;
  std::vector<std::unique_ptr<RenderAxisDeathContext>> axis_death_contexts;
  std::unordered_set<TreeInstanceHandle, TreeInstanceHandle::Hash> pending_deletion;
  std::vector<InsertedAttractionPoints> inserted_attraction_points;
  DeletedInstances just_deleted;

  bounds::ElementTag bounds_tree_element_tag{bounds::ElementTag::create()};
  bounds::ElementTag bounds_leaf_element_tag{bounds::ElementTag::create()};
};

TreeInstanceHandle create_tree(TreeSystem* sys, CreateTreeParams&& params, TreeID* id);
void destroy_tree(TreeSystem* sys, TreeInstanceHandle handle);
TreeSystem::ReadInstance read_tree(const TreeSystem* sys, TreeInstanceHandle handle);
bool tree_exists(const TreeSystem* sys, TreeInstanceHandle handle);
void prepare_to_grow(TreeSystem* sys, TreeInstanceHandle handle,
                     const TreeSystem::PrepareToGrowParams& params);
void finish_growing(TreeSystem* sys, TreeInstanceHandle handle);

void start_render_growing(TreeSystem* sys, TreeInstanceHandle handle);
void finish_render_growing(TreeSystem* sys, TreeInstanceHandle handle);

void start_render_dying(TreeSystem* sys, TreeInstanceHandle handle);
void set_axis_growth_increment(TreeSystem* sys, TreeInstanceHandle handle, float incr);

bool can_start_pruning(const TreeSystem* sys, TreeInstanceHandle handle);
void start_pruning(TreeSystem* sys, TreeInstanceHandle handle, TreeSystem::PruningData&& data);
void finish_pruning_leaves(TreeSystem* sys, TreeInstanceHandle handle);
void finish_pruning(TreeSystem* sys, TreeInstanceHandle handle);

Optional<TreeInstanceHandle>
lookup_instance_by_bounds_element_id(const TreeSystem* sys, bounds::ElementID id);

bool lookup_by_bounds_element_ids(const TreeSystem* sys, bounds::ElementID tree_id,
                                  bounds::ElementID internode_id, TreeInstanceHandle* tree_handle,
                                  tree::Internode* internode, int* internode_index);

bounds::ElementTag get_bounds_tree_element_tag(const TreeSystem* sys);
bounds::ElementTag get_bounds_leaf_element_tag(const TreeSystem* sys);
bounds::RadiusLimiterElementTag get_tree_radius_limiter_element_tag(const TreeSystem* sys);

[[nodiscard]] TreeSystem::UpdateResult update(TreeSystem* sys, const TreeSystem::UpdateInfo& info);

TreeSystem::Stats get_stats(const TreeSystem* sys);

}