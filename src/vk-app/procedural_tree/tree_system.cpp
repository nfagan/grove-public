#include "tree_system.hpp"
#include "bounds.hpp"
#include "render.hpp"
#include "bud_fate.hpp"
#include "utility.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/common/profile.hpp"
#include "grove/common/logging.hpp"
#include "grove/common/Stopwatch.hpp"

GROVE_NAMESPACE_BEGIN

#define USE_NEW_RENDER_GROWTH_UPDATE (0)
#define ENABLE_LIMITING_FINISH_GENERATING_NODE_STRUCTURE_PER_FRAME (1)
#define MAX_NUM_GENERATE_NODE_STRUCTURE_PER_FRAME (8)

namespace {

using namespace tree;
using Instance = TreeSystem::Instance;
using UpdateInfo = TreeSystem::UpdateInfo;
using UpdateResult = TreeSystem::UpdateResult;
using ModifyingState = TreeSystem::ModifyingState;
using ModifyingPhase = TreeSystem::ModifyingPhase;
using PruningData = TreeSystem::PruningData;
using BoundsElementIDs = std::vector<bounds::ElementID>;

struct Config {
  static bounds::RadiusLimiterElementTag tree_tag;
  static constexpr double reference_dt = 1.0 / 60.0;
};

struct {
  double max_time_spent_state_growing_s{};
  double max_time_spent_finish_generating_node_structure_s{};
  double max_time_spent_pruning_against_radius_limiter_s{};
  int max_num_instances_generated_node_structure_in_one_frame{};
} globals;

bounds::RadiusLimiterElementTag Config::tree_tag = bounds::RadiusLimiterElementTag::create();

template <typename T>
std::unique_ptr<T> require_context(std::vector<std::unique_ptr<T>>& ctxs) {
  if (!ctxs.empty()) {
    auto res = std::move(ctxs.back());
    ctxs.pop_back();
    return res;
  } else {
    return std::make_unique<T>();
  }
}

std::unique_ptr<RenderAxisDeathContext> require_death_context(TreeSystem* sys) {
  return require_context(sys->axis_death_contexts);
}

std::unique_ptr<RenderAxisGrowthContext> require_growth_context(TreeSystem* sys) {
  return require_context(sys->axis_growth_contexts);
}

void return_growth_context(TreeSystem* sys, std::unique_ptr<RenderAxisGrowthContext> ctx) {
  sys->axis_growth_contexts.push_back(std::move(ctx));
}

void return_death_context(TreeSystem* sys, std::unique_ptr<RenderAxisDeathContext> ctx) {
  sys->axis_death_contexts.push_back(std::move(ctx));
}

[[maybe_unused]] void validate_pruning_data(const PruningData& data) {
  if (data.internodes) {
    auto& nodes = data.internodes.value();
    assert(nodes.internodes.size() == nodes.dst_to_src.size());
    (void) nodes;
  }
}

Instance make_instance(CreateTreeParams&& params) {
  Instance result{};
  result.nodes = make_tree_node_store(params.origin, params.spawn_params);
  result.spawn_params = std::move(params.spawn_params);
  result.bud_q_params = std::move(params.bud_q_params);
  result.make_attraction_points = std::move(params.make_attraction_points);
  result.insert_into_accel = params.insert_into_accel;
  result.bounds_element_id = bounds::ElementID::create();
  result.leaves.internode_bounds_scale = params.leaf_internode_bounds_scale;
  result.leaves.internode_bounds_offset = params.leaf_internode_bounds_offset;
  result.leaves.bounds_distribution_strategy = params.leaf_bounds_distribution_strategy;
  return result;
}

PrepareToGrowParams to_prepare_to_grow_params(Instance* inst) {
  PrepareToGrowParams result;
  result.context = inst->prepare_to_grow_params.context;
  result.nodes = std::move(inst->nodes);
  result.spawn_params = std::move(inst->spawn_params);
  result.bud_q_params = std::move(inst->bud_q_params);
  result.make_attraction_points = std::move(inst->make_attraction_points);
  result.max_num_internodes = inst->prepare_to_grow_params.max_num_internodes;
  return result;
}

AccelInsertAndPruneParams
to_internode_accel_insert_and_prune_params(const TreeSystem* sys, Instance&& inst) {
  AccelInsertAndPruneParams result{};
  result.internodes = std::move(inst.nodes.internodes);
  result.tree_element_tag = sys->bounds_tree_element_tag;
  result.leaf_element_tag = sys->bounds_leaf_element_tag;
  result.parent_element_id = inst.bounds_element_id;
  result.accel = inst.insert_into_accel.value();
  return result;
}

AccelInsertAndPruneParams
to_leaf_accel_insert_params(const TreeSystem* sys, const Instance& inst,
                            std::vector<OBB3f>&& bounds) {
  AccelInsertAndPruneParams result{};
  result.leaf_bounds = std::move(bounds);
  result.tree_element_tag = sys->bounds_tree_element_tag;
  result.leaf_element_tag = sys->bounds_leaf_element_tag;
  result.parent_element_id = inst.bounds_element_id;
  result.accel = inst.insert_into_accel.value();
  return result;
}

void move_from_growth_result(Instance* inst, GrowthContextHandle* grown_context) {
  auto& growth_res = inst->future_growth_result->data;
  *grown_context = growth_res.context_handle;
  inst->nodes = std::move(growth_res.nodes);
  inst->spawn_params = std::move(growth_res.spawn_params);
  inst->bud_q_params = std::move(growth_res.bud_q_params);
  inst->make_attraction_points = std::move(growth_res.make_attraction_points);
  inst->future_growth_result = nullptr;
}

void move_from_future_internode_insert_and_prune_result(Instance* inst) {
  //  @TODO: Hold onto additional result data so that nodes can be deleted from accel.
  auto& res = inst->future_insert_and_prune_result->data;
  inst->nodes.internodes = std::move(res.pruned_internodes);
  inst->inserted_internode_bounds = std::move(res.pruned_internode_element_ids);
  inst->future_insert_and_prune_result = nullptr;
}

void move_from_future_leaf_insert_result(Instance* inst) {
  auto& res = inst->future_insert_and_prune_result->data;
  inst->leaves.inserted_bounds = std::move(res.pruned_leaf_element_ids);
  inst->future_insert_and_prune_result = nullptr;
}

Instance* find_instance(TreeSystem* sys, TreeInstanceHandle handle) {
  auto it = sys->instances.find(handle.id);
  return it == sys->instances.end() ? nullptr : &it->second;
}

const Instance* find_instance(const TreeSystem* sys, TreeInstanceHandle handle) {
  auto it = sys->instances.find(handle.id);
  return it == sys->instances.end() ? nullptr : &it->second;
}

std::vector<OBB3f> gather_leaf_internode_bounds_original(const Internodes& inodes,
                                                         const Vec3f& scale, const Vec3f& off) {
  std::vector<OBB3f> result;
  for (auto& node : inodes) {
    if (node.is_leaf()) {
      result.push_back(tree::internode_relative_obb(node, scale, off));
    }
  }
  return result;
}

std::vector<OBB3f> gather_leaf_internode_bounds_axis_aligned_outwards(
  const Internodes& inodes, const Vec3f& scale, const Vec3f& off) {
  //
  const auto node_aabb = tree::internode_aabb(inodes);
  std::vector<OBB3f> result;
  for (auto& node : inodes) {
    if (node.is_leaf()) {
      auto leaf_dir = node.position - node_aabb.center();
      auto leaf_dir_xz = Vec3f{leaf_dir.x, 0.0f, leaf_dir.z};
      leaf_dir_xz = normalize_or_default(leaf_dir_xz, Vec3f{1.0f, 0.0f, 0.0f});
      auto leaf_p = node.position + leaf_dir_xz * off;
      auto leaf_obb = OBB3f::axis_aligned(leaf_p, scale);
      result.push_back(leaf_obb);
    }
  }
  return result;
}

std::vector<OBB3f> gather_leaf_internode_bounds(const Instance& instance) {
  switch (instance.leaves.bounds_distribution_strategy) {
    case TreeSystemLeafBoundsDistributionStrategy::Original: {
      return gather_leaf_internode_bounds_original(
        instance.nodes.internodes,
        instance.leaves.internode_bounds_scale,
        instance.leaves.internode_bounds_offset);
    }
    case TreeSystemLeafBoundsDistributionStrategy::AxisAlignedOutwardsFromNodes: {
      return gather_leaf_internode_bounds_axis_aligned_outwards(
        instance.nodes.internodes,
        instance.leaves.internode_bounds_scale,
        instance.leaves.internode_bounds_offset);
    }
    default: {
      assert(false);
      return {};
    }
  }
}

bool any_pending_modifications(const TreeSystem::GrowthState& state) {
  return state.pending_prune || state.pending_render_death ||
         state.pending_render_growth || state.pending_growth;
}

[[maybe_unused]] bool is_idle(ModifyingPhase phase) {
  return phase == ModifyingPhase::Idle;
}

bool is_idle(ModifyingState state) {
  return state == ModifyingState::Idle;
}

bool is_growing(ModifyingState state) {
  return state == ModifyingState::Growing;
}

bool is_pruning(ModifyingState state) {
  return state == ModifyingState::Pruning;
}

bool is_awaiting_finish_growing_signal(ModifyingPhase phase) {
  return phase == ModifyingPhase::AwaitingFinishGrowingSignal;
}

bool can_start_modifying_nodes(const Instance& instance) {
  return is_idle(instance.growth_state.modifying);
}

bool can_read_nodes(const Instance& instance) {
  auto state = instance.growth_state.modifying;
  auto phase = instance.growth_state.phase;
  return !is_growing(state) ||
         (is_growing(state) && is_awaiting_finish_growing_signal(phase));
}

bool can_destroy_now(const Instance& instance) {
  return instance.growth_state.modifying != ModifyingState::Growing;
}

bool can_start_pruning(const Instance& instance) {
  auto& state = instance.growth_state;
  return can_start_modifying_nodes(instance) && !any_pending_modifications(state);
}

float dt_scaled_growth_incr(float incr, double dt) {
  return incr * float(dt / Config::reference_dt);
}

#if USE_NEW_RENDER_GROWTH_UPDATE
void prepare_internodes_for_render_growth_with_new_growth_update(Internodes& inodes) {
  for (auto& node : inodes) {
    node.length_scale = 0.0f;
    node.diameter = 0.0f;
    node.render_position = node.position;
  }
}
#endif

void start_render_growth(TreeSystem* sys, Instance& inst) {
  assert(!inst.axis_growth_context);
  inst.axis_growth_context = require_growth_context(sys);
  auto& inodes = inst.nodes.internodes;
  tree::initialize_axis_render_growth_context(inst.axis_growth_context.get(), inodes, 0);
#if USE_NEW_RENDER_GROWTH_UPDATE
  prepare_internodes_for_render_growth_with_new_growth_update(inodes);
#else
  tree::set_render_length_scale(inodes, 0, 0.0f);
#endif
  inst.events.node_render_position_modified = true;
  inst.events.just_started_render_growing = true;
}

void start_render_death(TreeSystem* sys, Instance& inst) {
  assert(!inst.axis_death_context);
  inst.axis_death_context = require_death_context(sys);
  *inst.axis_death_context = tree::make_default_render_axis_death_context(
    inst.nodes.internodes);
}

void start_pruning_internodes(TreeSystem* sys, Instance& inst) {
  assert(inst.pruning_data && inst.pruning_data->internodes && !inst.axis_death_context);
  auto& pruning_internodes = inst.pruning_data->internodes.value();
  inst.axis_death_context = require_death_context(sys);
  tree::initialize_axis_pruning(
    inst.axis_death_context.get(),
    inst.nodes.internodes,
    std::unordered_set<int>{
      pruning_internodes.dst_to_src.begin(),
      pruning_internodes.dst_to_src.end()});
  inst.growth_state.phase = ModifyingPhase::PruningInternodes;
}

void start_awaiting_finish_pruning_signal(Instance& inst) {
  inst.growth_state.phase = ModifyingPhase::AwaitingFinishPruningSignal;
  inst.events.just_started_awaiting_finish_pruning_signal = true;
}

void on_finish_pruning_leaves(TreeSystem* sys, Instance& inst) {
  if (inst.pruning_data->internodes) {
    start_pruning_internodes(sys, inst);
  } else {
    //  No internodes to prune, so await finish.
    start_awaiting_finish_pruning_signal(inst);
  }
}

void partition_element_ids(const BoundsElementIDs& ids, const std::unordered_set<int>& keep,
                           BoundsElementIDs& removed, BoundsElementIDs& kept) {
  for (int i = 0; i < int(ids.size()); i++) {
    if (keep.count(i) == 0) {
      removed.push_back(ids[i]);
    } else {
      kept.push_back(ids[i]);
    }
  }
}

BoundsElementIDs prune_accel_bounds(bounds::BoundsSystem* sys,
                                    bounds::AccelInstanceHandle accel,
                                    const Instance& inst, const std::unordered_set<int>& keep,
                                    int new_num_internodes) {
  const auto& curr_inodes = inst.nodes.internodes;
  auto& curr_inode_bounds = inst.inserted_internode_bounds;
  assert(curr_inodes.size() == curr_inode_bounds.size() && int(keep.size()) == new_num_internodes);
  (void) new_num_internodes;
  (void) curr_inodes;

  std::vector<bounds::ElementID> removed;
  std::vector<bounds::ElementID> kept;
  partition_element_ids(curr_inode_bounds, keep, removed, kept);
  bounds::push_pending_deactivation(sys, accel, std::move(removed));
  return kept;
}

bool already_registered_inserted_attraction_points(TreeSystem* sys, TreeID id,
                                                   GrowthContextHandle context) {
  for (auto& inserted : sys->inserted_attraction_points) {
    if (inserted.id == id && inserted.context == context) {
      return true;
    }
  }
  return false;
}

void register_inserted_attraction_points(TreeSystem* sys, tree::TreeID id,
                                         GrowthContextHandle context) {
  assert(context.is_valid() && id.is_valid());
  if (!already_registered_inserted_attraction_points(sys, id, context)) {
    TreeSystem::InsertedAttractionPoints inserted{};
    inserted.id = id;
    inserted.context = context;
    sys->inserted_attraction_points.push_back(inserted);
  }
}

void finish_pruning_internodes(TreeSystem* sys, bounds::BoundsSystem* bounds_sys, Instance& inst) {
  assert(inst.pruning_data && inst.pruning_data->internodes);
  auto& dst_internodes = inst.pruning_data->internodes.value().internodes;
  if (inst.insert_into_accel) {
    inst.inserted_internode_bounds = prune_accel_bounds(
      bounds_sys,
      inst.insert_into_accel.value(),
      inst,
      inst.axis_death_context->preserve,
      int(dst_internodes.size()));
    assert(dst_internodes.size() == inst.inserted_internode_bounds.size());
  }
  return_death_context(sys, std::move(inst.axis_death_context));
  inst.nodes.internodes = std::move(dst_internodes);
  inst.events.node_structure_modified = true;
  start_awaiting_finish_pruning_signal(inst);
}

void on_finish_pruning(TreeSystem*, Instance& inst) {
  assert(is_pruning(inst.growth_state.modifying) &&
         inst.growth_state.phase == ModifyingPhase::FinishedPruningSignalReceived &&
         inst.pruning_data);
  inst.growth_state.phase = ModifyingPhase::Idle;
  inst.growth_state.modifying = ModifyingState::Idle;
  inst.events.just_finished_pruning = true;
  inst.pruning_data = nullptr;
}

void on_finish_generating_node_structure(TreeSystem* sys, Instance* inst) {
  assert(inst->growth_state.phase == ModifyingPhase::GeneratingNodeStructure);
  GrowthContextHandle grown_from_context{};
  move_from_growth_result(inst, &grown_from_context);
  register_inserted_attraction_points(sys, inst->nodes.id, grown_from_context);
  inst->growth_state.phase = ModifyingPhase::Idle;
}

void on_start_awaiting_finish_growing_signal(Instance* inst) {
  //  @NOTE: By re-setting the diameter here, an internode's bounding box might change. It will
  //  change if the internode is upstream of an axis that was pruned because it was found
  //  to intersect with other bounding boxes in the acceleration structure. If it does change, then
  //  this now canonical bounding box will differ from the one inserted into
  //  the acceleration structure during growth. It's therefore important to not assume that these
  //  quantities are the same.
  //
  //  We could avoid setting the diameter here, in which case the bounding boxes would temporarily
  //  match. But other routines (like `update_render_growth`) also call `set_diameter`, so we should
  //  do it here to avoid a potential rendering discontinuity later.
  assert(is_growing(inst->growth_state.modifying));
  tree::set_diameter(inst->nodes.internodes, inst->spawn_params);
  tree::copy_diameter_to_lateral_q(inst->nodes.internodes);
#if 1 //  @NOTE: 10/28/22
  tree::prefer_larger_axes(inst->nodes.internodes.data(), int(inst->nodes.internodes.size()));
  tree::reassign_gravelius_order(inst->nodes.internodes.data(), int(inst->nodes.internodes.size()));
#endif
  inst->src_aabb = tree::internode_aabb(inst->nodes.internodes);
  inst->growth_state.phase = ModifyingPhase::AwaitingFinishGrowingSignal;
  inst->events.node_structure_modified = true;
  inst->events.just_started_awaiting_finish_growth_signal = true;
}

double prune_intersecting_radius_limiter(TreeSystem*, Instance* inst,
                                         bounds::RadiusLimiter* lim,
                                         bounds::RadiusLimiterElementTag roots_tag) {
  Stopwatch t0;

  const int num_nodes = int(inst->nodes.internodes.size());
  const auto* src_nodes = inst->nodes.internodes.data();

  Temporary<bool, 1024> store_accept_node;
  bool* accept_node = store_accept_node.require(num_nodes);
  std::fill(accept_node, accept_node + num_nodes, false);

  Temporary<bounds::RadiusLimiterElementHandle, 1024> store_handles;
  auto* inserted_elements = store_handles.require(num_nodes);

  //  @NOTE: Store this somewhere?
  const auto aggregate_id = bounds::RadiusLimiterAggregateID::create();

  //  @HACK: Don't insert the root node's bounds because it can interfere with newly spawned roots
  //  growing right below it.
  int axis_root_index{};
  bool lock_root_node_direction{};
  Vec3f locked_root_node_direction{};
#if 1
  if (num_nodes > 1 && src_nodes[0].has_medial_child()) {
    accept_node[0] = true;
    axis_root_index = src_nodes[0].medial_child;
    lock_root_node_direction = true;
    locked_root_node_direction = src_nodes[0].direction;
  }
#endif

  tree::PruneIntersectingRadiusLimiterParams prune_params{};
  prune_params.nodes = src_nodes;
  prune_params.root_index = axis_root_index;
  prune_params.num_nodes = num_nodes;
  prune_params.lim = lim;
  prune_params.aggregate_id = &aggregate_id;
  prune_params.roots_tag = &roots_tag;
  prune_params.tree_tag = &Config::tree_tag;
  prune_params.accept_node = accept_node;
  prune_params.inserted_elements = inserted_elements;
  prune_params.lock_root_node_direction = lock_root_node_direction;
  prune_params.locked_root_node_direction = locked_root_node_direction;
  const int num_inserted = tree::prune_intersecting_radius_limiter(prune_params);

  inst->inserted_radius_limiter_elements.resize(num_inserted);
  std::copy(
    inserted_elements,
    inserted_elements + num_inserted,
    inst->inserted_radius_limiter_elements.data());

  if (!std::all_of(accept_node, accept_node + num_nodes, [](bool v){ return v; })) {
    std::vector<tree::Internode> dst_nodes(num_nodes);
    const int num_kept = tree::prune_rejected_axes(
      src_nodes, accept_node, num_nodes, dst_nodes.data(), nullptr);
    dst_nodes.resize(num_kept);
    inst->nodes.internodes = std::move(dst_nodes);
#if 1
    {
      std::string msg{"Pruned "};
      msg += std::to_string(num_nodes - num_kept);
      msg += " nodes.";
      GROVE_LOG_SEVERE_CAPTURE_META(msg.c_str(), "TreeSystem");
    }
#endif
  }

  return t0.delta().count();
}

void remove_inserted_radius_limiter_elements(Instance* inst, bounds::RadiusLimiter* lim) {
  for (auto& el : inst->inserted_radius_limiter_elements) {
    bounds::remove(lim, el);
  }
  inst->inserted_radius_limiter_elements.clear();
}

void insert_internode_pending_accel_removal(bounds::BoundsSystem* sys, Instance* inst) {
  bounds::push_pending_deactivation(
    sys,
    inst->insert_into_accel.value(),
    std::move(inst->inserted_internode_bounds));
}

void insert_leaf_pending_accel_removal(bounds::BoundsSystem* sys, Instance* inst) {
  bounds::push_pending_deactivation(
    sys,
    inst->insert_into_accel.value(),
    std::move(inst->leaves.inserted_bounds));
}

void push_pending_accel_removal(bounds::BoundsSystem* sys, Instance* inst) {
  insert_internode_pending_accel_removal(sys, inst);
  insert_leaf_pending_accel_removal(sys, inst);
}

void push_pending_attraction_points_clear(TreeSystem* sys, GrowthSystem2* growth_sys,
                                          const Instance* inst) {
  auto it = sys->inserted_attraction_points.begin();
  while (it != sys->inserted_attraction_points.end()) {
    const TreeSystem::InsertedAttractionPoints& pts = *it;
    if (pts.id == inst->nodes.id) {
      push_pending_attraction_points_clear(growth_sys, pts.context, pts.id);
      it = sys->inserted_attraction_points.erase(it);
    } else {
      ++it;
    }
  }
}

void on_destroy(TreeSystem* sys, Instance* inst, const UpdateInfo& info) {
  push_pending_accel_removal(info.bounds_system, inst);
  push_pending_attraction_points_clear(sys, info.growth_system, inst);
#if GROVE_INCLUDE_TREE_INTERNODES_IN_RADIUS_LIMITER
  remove_inserted_radius_limiter_elements(inst, info.radius_limiter);
#endif
}

void update_pending_deletion(TreeSystem* sys, const UpdateInfo& info) {
  sys->just_deleted.clear();

  auto del_it = sys->pending_deletion.begin();
  while (del_it != sys->pending_deletion.end()) {
    const TreeInstanceHandle id = *del_it;
    auto inst_it = sys->instances.find(id.id);
    assert(inst_it != sys->instances.end());
    auto& inst = inst_it->second;

    if (can_destroy_now(inst)) {
      sys->just_deleted.insert(id);
      on_destroy(sys, &inst, info);
      sys->instances.erase(inst_it);
      del_it = sys->pending_deletion.erase(del_it);
    } else {
      ++del_it;
    }
  }
}

void push_internode_accel_insert_and_prune(TreeSystem* sys, Instance* inst,
                                           const UpdateInfo& info) {
  assert(!inst->future_insert_and_prune_result);
  auto accel_params = to_internode_accel_insert_and_prune_params(sys, std::move(*inst));
  inst->future_insert_and_prune_result = push_internode_accel_insert_and_prune(
    info.accel_insert_and_prune,
    std::move(accel_params));
}

void push_leaf_accel_insert(TreeSystem* sys, Instance* inst, const UpdateInfo& info) {
  assert(!inst->future_insert_and_prune_result);
  auto bounds = gather_leaf_internode_bounds(*inst);
  auto accel_params = to_leaf_accel_insert_params(sys, *inst, std::move(bounds));
  inst->future_insert_and_prune_result = push_leaf_accel_insert(
    info.accel_insert_and_prune,
    std::move(accel_params));
}

void prune_leaf_bounds(bounds::BoundsSystem* sys, bounds::AccelInstanceHandle accel,
                       BoundsElementIDs& inserted, BoundsElementIDs&& pruning) {
  for (const bounds::ElementID id : pruning) {
    if (auto it = std::find(inserted.begin(), inserted.end(), id); it != inserted.end()) {
      inserted.erase(it);
    }
  }
  bounds::push_pending_deactivation(sys, accel, std::move(pruning));
}

void prune_leaf_bounds(bounds::BoundsSystem* sys, Instance* inst) {
  assert(inst->pruning_data && inst->insert_into_accel);
  auto& inserted = inst->leaves.inserted_bounds;
  auto& pruning = inst->pruning_data->leaves.remove_bounds;
  prune_leaf_bounds(sys, inst->insert_into_accel.value(), inserted, std::move(pruning));
}

void start_pruning_leaves(Instance* inst, const UpdateInfo& info) {
  auto& leaves = inst->pruning_data->leaves;
  if (!leaves.remove_bounds.empty()) {
    prune_leaf_bounds(info.bounds_system, inst);
    assert(leaves.remove_bounds.empty());
  }
}

void start_pruning(TreeSystem*, Instance* inst, const UpdateInfo& info) {
  assert(is_idle(inst->growth_state.phase) && is_idle(inst->growth_state.modifying) &&
         inst->growth_state.pending_prune && inst->pruning_data);
  inst->growth_state.pending_prune = false;
  inst->growth_state.modifying = ModifyingState::Pruning;
  inst->growth_state.phase = ModifyingPhase::AwaitingFinishPruningLeavesSignal;
  inst->events.just_started_pruning = true;
  start_pruning_leaves(inst, info);
}

void update_growth(TreeSystem* sys, const UpdateInfo& info) {
  for (auto& [_, inst] : sys->instances) {
    if (inst.growth_state.pending_growth && can_start_modifying_nodes(inst)) {
      assert(!inst.future_growth_result);
      assert(is_idle(inst.growth_state.modifying) && is_idle(inst.growth_state.phase));
      inst.future_growth_result = prepare_to_grow(
        info.growth_system,
        to_prepare_to_grow_params(&inst));
      inst.growth_state.pending_growth = false;
      inst.growth_state.modifying = ModifyingState::Growing;
      inst.growth_state.phase = ModifyingPhase::GeneratingNodeStructure;
    }
  }

  Stopwatch t0;
  Stopwatch t1;
  double generating_structure_t{};
  double pruning_against_radius_limiter_t{};
  int num_finished_generating_structure{};

  for (auto& [id, inst] : sys->instances) {
    if (inst.growth_state.modifying != ModifyingState::Growing) {
      continue;
    }
    if (inst.growth_state.phase == ModifyingPhase::GeneratingNodeStructure) {
      assert(inst.future_growth_result);
#if ENABLE_LIMITING_FINISH_GENERATING_NODE_STRUCTURE_PER_FRAME
      if (inst.future_growth_result->is_ready() &&
          num_finished_generating_structure < MAX_NUM_GENERATE_NODE_STRUCTURE_PER_FRAME) {
#else
      if (inst.future_growth_result->is_ready()) {
#endif
        t0.reset();

        on_finish_generating_node_structure(sys, &inst);
#if GROVE_INCLUDE_TREE_INTERNODES_IN_RADIUS_LIMITER
        pruning_against_radius_limiter_t += prune_intersecting_radius_limiter(
          sys, &inst, info.radius_limiter, info.roots_tag);
#endif

        if (inst.insert_into_accel) {
          push_internode_accel_insert_and_prune(sys, &inst, info);
          inst.growth_state.phase = ModifyingPhase::NodeAccelInsertingAndPruning;
        } else {
          on_start_awaiting_finish_growing_signal(&inst);
        }

        generating_structure_t += t0.delta().count();
        num_finished_generating_structure++;
      }
    }
    if (inst.growth_state.phase == ModifyingPhase::NodeAccelInsertingAndPruning) {
      if (inst.future_insert_and_prune_result->is_ready()) {
        insert_internode_pending_accel_removal(info.bounds_system, &inst);
        move_from_future_internode_insert_and_prune_result(&inst);
        inst.growth_state.phase = ModifyingPhase::Idle;

        push_leaf_accel_insert(sys, &inst, info);
        inst.growth_state.phase = ModifyingPhase::LeafAccelInserting;
      }
    }
    if (inst.growth_state.phase == ModifyingPhase::LeafAccelInserting) {
      if (inst.future_insert_and_prune_result->is_ready()) {
        insert_leaf_pending_accel_removal(info.bounds_system, &inst);
        move_from_future_leaf_insert_result(&inst);
        on_start_awaiting_finish_growing_signal(&inst);
      }
    }
    if (inst.growth_state.phase == ModifyingPhase::FinishedGrowingSignalReceived) {
      inst.growth_state.modifying = ModifyingState::Idle;
      inst.growth_state.phase = ModifyingPhase::Idle;
    }
  }

  globals.max_time_spent_finish_generating_node_structure_s = std::max(
    generating_structure_t, globals.max_time_spent_finish_generating_node_structure_s);
  globals.max_time_spent_state_growing_s = std::max(
    t1.delta().count(), globals.max_time_spent_state_growing_s);
  globals.max_time_spent_pruning_against_radius_limiter_s = std::max(
    pruning_against_radius_limiter_t,
    globals.max_time_spent_pruning_against_radius_limiter_s);
  globals.max_num_instances_generated_node_structure_in_one_frame = std::max(
    num_finished_generating_structure,
    globals.max_num_instances_generated_node_structure_in_one_frame);
}

void update_render_growth(TreeSystem* sys, const UpdateInfo& info) {
  for (auto& [_, inst] : sys->instances) {
    if (inst.growth_state.pending_render_growth && can_start_modifying_nodes(inst)) {
      start_render_growth(sys, inst);
      inst.growth_state.pending_render_growth = false;
      inst.growth_state.modifying = ModifyingState::RenderGrowing;
      inst.growth_state.phase = ModifyingPhase::RenderGrowing;
    }
  }

  for (auto& [_, inst] : sys->instances) {
    if (inst.growth_state.modifying != ModifyingState::RenderGrowing) {
      continue;
    }
    if (inst.growth_state.phase == ModifyingPhase::RenderGrowing) {
      float growth_incr = dt_scaled_growth_incr(inst.axis_growth_incr, info.real_dt);
      if (growth_incr > 0.0f) {
        inst.events.node_render_position_modified = true;
#if USE_NEW_RENDER_GROWTH_UPDATE
        bool still_growing = tree::update_render_growth_src_diameter_in_lateral_q(
          inst.nodes.internodes, *inst.axis_growth_context, inst.spawn_params, growth_incr);
#else
        bool still_growing = tree::update_render_growth(
          inst.nodes.internodes,
          inst.spawn_params,
          *inst.axis_growth_context,
          growth_incr);
#endif
        if (!still_growing) {
          return_growth_context(sys, std::move(inst.axis_growth_context));
          inst.growth_state.phase = ModifyingPhase::AwaitingFinishRenderGrowingSignal;
          inst.events.just_started_awaiting_finish_render_growth_signal = true;
        }
      }
    }
    if (inst.growth_state.phase == ModifyingPhase::FinishedRenderGrowingSignalReceived) {
      inst.growth_state.phase = ModifyingPhase::Idle;
      inst.growth_state.modifying = ModifyingState::Idle;
    }
  }
}

void update_render_death(TreeSystem* sys, const UpdateInfo& info) {
  for (auto& [_, inst] : sys->instances) {
    if (inst.growth_state.pending_render_death && can_start_modifying_nodes(inst)) {
      start_render_death(sys, inst);
      inst.growth_state.pending_render_death = false;
      inst.growth_state.modifying = ModifyingState::RenderDying;
    }
  }

  for (auto& [_, inst] : sys->instances) {
    if (inst.growth_state.modifying == ModifyingState::RenderDying) {
      //  @TODO: Separate death increment.
      float death_incr = dt_scaled_growth_incr(inst.axis_growth_incr, info.real_dt);
      if (death_incr > 0.0f) {
        inst.events.node_render_position_modified = true;
#if 1
        bool still_dying = tree::update_render_death_src_diameter_in_lateral_q(
          inst.nodes.internodes, *inst.axis_death_context, death_incr);
#else
        bool still_dying = tree::update_render_death(
          inst.nodes.internodes,
          inst.spawn_params,
          *inst.axis_death_context,
          death_incr);
#endif
        if (!still_dying) {
          return_death_context(sys, std::move(inst.axis_death_context));
          inst.growth_state.modifying = ModifyingState::Idle;
          inst.events.just_finished_render_death = true;
        }
      }
    }
  }
}

void update_pruning(TreeSystem* sys, const UpdateInfo& info) {
  for (auto& [_, inst] : sys->instances) {
    if (inst.growth_state.pending_prune && can_start_modifying_nodes(inst)) {
      start_pruning(sys, &inst, info);
    }

    if (!is_pruning(inst.growth_state.modifying)) {
      continue;
    }

    if (inst.growth_state.phase == ModifyingPhase::FinishedPruningLeavesSignalReceived) {
      on_finish_pruning_leaves(sys, inst);
    }

    if (inst.growth_state.phase == ModifyingPhase::PruningInternodes) {
      //  @TODO: Separate prune increment.
      float prune_incr = dt_scaled_growth_incr(inst.axis_growth_incr, info.real_dt);
      if (prune_incr > 0.0f) {
        inst.events.node_render_position_modified = true;
        bool still_pruning = tree::update_render_prune(
          inst.nodes.internodes, *inst.axis_death_context, prune_incr);
        if (!still_pruning) {
          finish_pruning_internodes(sys, info.bounds_system, inst);
        }
      }
    }

    if (inst.growth_state.phase == ModifyingPhase::FinishedPruningSignalReceived) {
      on_finish_pruning(sys, inst);
    }
  }
}

} //  anon

TreeInstanceHandle tree::create_tree(TreeSystem* sys, CreateTreeParams&& params, TreeID* dst_id) {
  uint32_t id{sys->next_instance_id++};
  auto inst = make_instance(std::move(params));
  *dst_id = inst.nodes.id;
  sys->instances[id] = std::move(inst);
  TreeInstanceHandle handle{id};
  return handle;
}

void tree::destroy_tree(TreeSystem* sys, TreeInstanceHandle handle) {
  sys->pending_deletion.insert(handle);
}

TreeSystem::ReadInstance tree::read_tree(const TreeSystem* sys, TreeInstanceHandle handle) {
  TreeSystem::ReadInstance result{};
  if (const auto* inst = find_instance(sys, handle)) {
    result.growth_state = inst->growth_state;
    result.events = inst->events;
    result.bounds_element_id = inst->bounds_element_id;
    if (can_read_nodes(*inst)) {
      result.nodes = &inst->nodes;
      result.src_aabb = &inst->src_aabb;
      result.leaves = &inst->leaves;
    }
  } else {
    assert(false);
  }
  return result;
}

bool tree::tree_exists(const TreeSystem* sys, TreeInstanceHandle handle) {
  return find_instance(sys, handle) != nullptr;
}

void tree::prepare_to_grow(TreeSystem* sys, TreeInstanceHandle handle,
                           const TreeSystem::PrepareToGrowParams& params) {
  if (auto* inst = find_instance(sys, handle)) {
    inst->growth_state.pending_growth = true;
    inst->prepare_to_grow_params = params;
  } else {
    assert(false);
  }
}

void tree::finish_growing(TreeSystem* sys, TreeInstanceHandle handle) {
  if (auto* inst = find_instance(sys, handle)) {
    assert(is_growing(inst->growth_state.modifying) &&
           is_awaiting_finish_growing_signal(inst->growth_state.phase));
    inst->growth_state.phase = ModifyingPhase::FinishedGrowingSignalReceived;
  } else {
    assert(false);
  }
}

void tree::start_render_growing(TreeSystem* sys, TreeInstanceHandle handle) {
  if (auto* inst = find_instance(sys, handle)) {
    inst->growth_state.pending_render_growth = true;
  } else {
    assert(false);
  }
}

void tree::finish_render_growing(TreeSystem* sys, TreeInstanceHandle handle) {
  if (auto* inst = find_instance(sys, handle)) {
    assert(inst->growth_state.modifying == ModifyingState::RenderGrowing &&
           inst->growth_state.phase == ModifyingPhase::AwaitingFinishRenderGrowingSignal);
    inst->growth_state.phase = ModifyingPhase::FinishedRenderGrowingSignalReceived;
  } else {
    assert(false);
  }
}

void tree::start_render_dying(TreeSystem* sys, TreeInstanceHandle handle) {
  if (auto* inst = find_instance(sys, handle)) {
    inst->growth_state.pending_render_death = true;
  } else {
    assert(false);
  }
}

UpdateResult tree::update(TreeSystem* sys, const UpdateInfo& info) {
  auto profiler = GROVE_PROFILE_SCOPE_TIC_TOC("TreeSystem/update");
  (void) profiler;

  UpdateResult result{};
  for (auto& [_, inst] : sys->instances) {
    inst.events = {};
  }

  //  @NOTE: Pruning has to take precedence over other modifications; it must come first here.
  grove::update_pruning(sys, info);
  grove::update_growth(sys, info);
  grove::update_render_growth(sys, info);
  grove::update_render_death(sys, info);
  grove::update_pending_deletion(sys, info);
  result.just_deleted = &sys->just_deleted;
  return result;
}

void tree::set_axis_growth_increment(TreeSystem* sys, TreeInstanceHandle handle, float incr) {
  if (auto* inst = find_instance(sys, handle)) {
    inst->axis_growth_incr = incr;
  } else {
    assert(false);
  }
}

bool tree::can_start_pruning(const TreeSystem* sys, TreeInstanceHandle handle) {
  if (auto* inst = find_instance(sys, handle)) {
    return grove::can_start_pruning(*inst);
  } else {
    assert(false);
    return false;
  }
}

void tree::finish_pruning_leaves(TreeSystem* sys, TreeInstanceHandle handle) {
  if (auto* inst = find_instance(sys, handle)) {
    assert(is_pruning(inst->growth_state.modifying) &&
           inst->growth_state.phase == ModifyingPhase::AwaitingFinishPruningLeavesSignal);
    inst->growth_state.phase = ModifyingPhase::FinishedPruningLeavesSignalReceived;
  } else {
    assert(false);
  }
}

void tree::finish_pruning(TreeSystem* sys, TreeInstanceHandle handle) {
  if (auto* inst = find_instance(sys, handle)) {
    assert(is_pruning(inst->growth_state.modifying) &&
           inst->growth_state.phase == ModifyingPhase::AwaitingFinishPruningSignal);
    inst->growth_state.phase = ModifyingPhase::FinishedPruningSignalReceived;
  } else {
    assert(false);
  }
}

void tree::start_pruning(TreeSystem* sys, TreeInstanceHandle handle,
                         TreeSystem::PruningData&& data) {
#ifdef GROVE_DEBUG
  validate_pruning_data(data);
#endif
  if (auto* inst = find_instance(sys, handle)) {
    assert(can_start_pruning(sys, handle) && !inst->pruning_data);
    inst->pruning_data = std::make_unique<PruningData>(std::move(data));
    inst->growth_state.pending_prune = true;
  } else {
    assert(false);
  }
}

Optional<TreeInstanceHandle> tree::lookup_instance_by_bounds_element_id(const TreeSystem* sys,
                                                                        bounds::ElementID id) {
  for (auto& [inst_id, inst] : sys->instances) {
    if (inst.bounds_element_id == id) {
      return Optional<TreeInstanceHandle>(TreeInstanceHandle{inst_id});
    }
  }
  return NullOpt{};
}

bool tree::lookup_by_bounds_element_ids(const TreeSystem* sys, bounds::ElementID tree_id,
                                        bounds::ElementID internode_id,
                                        TreeInstanceHandle* tree_handle,
                                        tree::Internode* internode,
                                        int* internode_index) {
  if (auto handle = lookup_instance_by_bounds_element_id(sys, tree_id)) {
    auto* inst = find_instance(sys, handle.value());
    if (can_read_nodes(*inst)) {
      assert(inst->inserted_internode_bounds.size() == inst->nodes.internodes.size());
      for (int i = 0; i < int(inst->inserted_internode_bounds.size()); i++) {
        if (inst->inserted_internode_bounds[i] == internode_id) {
          *tree_handle = handle.value();
          *internode = inst->nodes.internodes[i];
          *internode_index = i;
          return true;
        }
      }
    }
  }
  return false;
}

bounds::ElementTag tree::get_bounds_tree_element_tag(const TreeSystem* sys) {
  return sys->bounds_tree_element_tag;
}

bounds::ElementTag tree::get_bounds_leaf_element_tag(const TreeSystem* sys) {
  return sys->bounds_leaf_element_tag;
}

bounds::RadiusLimiterElementTag tree::get_tree_radius_limiter_element_tag(const TreeSystem*) {
  assert(Config::tree_tag.tag > 0);
  return Config::tree_tag;
}

TreeSystem::Stats tree::get_stats(const TreeSystem* sys) {
  TreeSystem::Stats result{};
  result.num_instances = int(sys->instances.size());
  result.num_axis_death_contexts = int(sys->axis_death_contexts.size());
  result.num_axis_growth_contexts = int(sys->axis_growth_contexts.size());
  result.num_pending_deletion = int(sys->pending_deletion.size());
  result.num_inserted_attraction_points = int(sys->inserted_attraction_points.size());
  result.max_time_spent_generating_node_structure_s =
    globals.max_time_spent_finish_generating_node_structure_s;
  result.max_time_spent_state_growing_s = globals.max_time_spent_state_growing_s;
  result.max_time_spent_pruning_against_radius_limiter_s =
    globals.max_time_spent_pruning_against_radius_limiter_s;
  result.max_num_instances_generated_node_structure_in_one_frame =
    globals.max_num_instances_generated_node_structure_in_one_frame;
  return result;
}

GROVE_NAMESPACE_END
