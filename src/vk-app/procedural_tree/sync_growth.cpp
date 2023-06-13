#include "sync_growth.hpp"
#include "environment_sample.hpp"
#include "environment_input.hpp"
#include "bud_fate.hpp"
#include "render.hpp"
#include "grove/common/common.hpp"
#include "grove/common/profile.hpp"
#include <cassert>

GROVE_NAMESPACE_BEGIN

using namespace tree;

namespace {

using Trees = ArrayView<TreeNodeStore>;
using BudStateTick = std::function<void(const TreeNodeStore&,
                                        const Bud&,
                                        GrowthCycleContext&,
                                        const GrowthCycleParams&)>;
using TreeStateTick = std::function<void(int,
                                         GrowthCycleContext&,
                                         const GrowthCycleParams&)>;

bool state_tick_per_bud(GrowthCycleContext& context,
                        const GrowthCycleParams& params,
                        const BudStateTick& tick_func) {
  while (true) {
    if (context.active_tree >= int(context.trees.size())) {
      context.active_tree = 0;
      return true;
    } else if (context.stopwatch.delta().count() >= params.time_limit_seconds) {
      return false;
    }

    assert(context.active_tree < int(context.trees.size()));
    auto* tree = context.trees[context.active_tree];
    auto& bud = tree->buds[context.active_bud++];

    if (context.active_bud == int(tree->buds.size())) {
      context.active_bud = 0;
      context.active_tree++;
    }

    tick_func(*tree, bud, context, params);
  }
}

bool state_tick_per_tree(GrowthCycleContext& context,
                         const GrowthCycleParams& params,
                         const TreeStateTick& tick_func) {
  while (true) {
    if (context.active_tree >= int(context.trees.size())) {
      context.active_tree = 0;
      return true;
    } else if (context.stopwatch.delta().count() >= params.time_limit_seconds) {
      return false;
    }

    assert(context.active_tree < int(context.trees.size()) &&
           context.active_tree < int(context.spawn_internode_params.size()) &&
           context.active_tree < int(context.distribute_bud_q_params.size()));

    tick_func(context.active_tree++, context, params);
  }
}

void state_consume_attraction_points(GrowthCycleContext& context,
                                     const GrowthCycleParams& params) {
  auto tick_func = [](const TreeNodeStore& nodes, auto& bud, auto& context, auto&&) {
    consume_within_occupancy_zone(nodes.id, bud, *context.attraction_points);
  };

  if (state_tick_per_bud(context, params, tick_func)) {
    context.state = GrowthState::SenseEnvironment;
  }
}

void state_sense(GrowthCycleContext& context, const GrowthCycleParams& params) {
  auto tick_func = [](auto&&, auto& bud, auto& context, auto&&) {
    sense_bud(bud, *context.attraction_points, context.sense_context);
  };

  if (state_tick_per_bud(context, params, tick_func)) {
    context.environment_input =
      compute_environment_input(context.sense_context.closest_points_to_buds);
    context.state = GrowthState::ApplyEnvironmentInput;
  }
}

void state_apply_environment_input(GrowthCycleContext& context,
                                   const GrowthCycleParams& params) {
  auto tick_func = [](auto tree_index, auto& context, auto&&) {
    auto& tree = context.trees[tree_index];
    auto& bud_q_params = context.distribute_bud_q_params[tree_index];
    apply_environment_input(*tree, context.environment_input, *bud_q_params);
  };

  if (state_tick_per_tree(context, params, tick_func)) {
    context.state = GrowthState::DetermineBudFate;
  }
}

void state_determine_bud_fate(GrowthCycleContext& context, const GrowthCycleParams& params) {
  auto tick_func = [](auto tree_index, auto& context, auto&&) {
    auto& tree = context.trees[tree_index];
    auto& spawn_params = context.spawn_internode_params[tree_index];
    bud_fate(*tree, context.environment_input, *spawn_params);
  };

  if (state_tick_per_tree(context, params, tick_func)) {
    context.state = GrowthState::SetRenderPosition;
  }
}

void state_set_render_position(GrowthCycleContext& context, const GrowthCycleParams& params) {
  auto tick_func = [](auto tree_index, auto& context, auto&&) {
    auto& tree = context.trees[tree_index];
    set_render_position(tree->internodes, 0);
  };

  if (state_tick_per_tree(context, params, tick_func)) {
    context.state = GrowthState::Idle;
  }
}

} //  anon

void tree::initialize_growth_cycle(GrowthCycleContext& context,
                                   AttractionPoints* attraction_points,
                                   std::vector<TreeNodeStore*>&& trees,
                                   std::vector<const SpawnInternodeParams*>&& spawn_params,
                                   std::vector<const DistributeBudQParams*>&& bud_q_params) {
  assert(trees.size() == spawn_params.size() && trees.size() == bud_q_params.size());
  context.trees = std::move(trees);
  context.spawn_internode_params = std::move(spawn_params);
  context.distribute_bud_q_params = std::move(bud_q_params);
  context.state = GrowthState::ConsumeAttractionPoints;
  context.active_tree = 0;
  context.active_bud = 0;
  context.environment_input.clear();
  context.sense_context.clear();
  context.attraction_points = attraction_points;
}

void tree::growth_cycle(GrowthCycleContext& context, const GrowthCycleParams& params) {
  if (context.state == GrowthState::Idle) {
    return;
  }

  auto profiler = GROVE_PROFILE_SCOPE_TIC_TOC("tree/growth_cycle");
  context.stopwatch.reset();

  if (context.state == GrowthState::ConsumeAttractionPoints) {
    state_consume_attraction_points(context, params);

  } else if (context.state == GrowthState::SenseEnvironment) {
    state_sense(context, params);

  } else if (context.state == GrowthState::ApplyEnvironmentInput) {
    state_apply_environment_input(context, params);

  } else if (context.state == GrowthState::DetermineBudFate) {
    state_determine_bud_fate(context, params);

  } else if (context.state == GrowthState::SetRenderPosition) {
    state_set_render_position(context, params);
  }
}

GROVE_NAMESPACE_END
