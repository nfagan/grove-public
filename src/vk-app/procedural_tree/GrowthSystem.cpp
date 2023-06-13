#include "GrowthSystem.hpp"
#include "environment_sample.hpp"
#include "environment_input.hpp"
#include "bud_fate.hpp"
#include "render.hpp"
#include "grove/common/common.hpp"
#include "grove/common/vector_util.hpp"
#include <chrono>

GROVE_NAMESPACE_BEGIN

namespace {

constexpr auto sleep_dur = std::chrono::milliseconds(20);

using Context = tree::GrowthSystem::Context;
using GrowableTree = tree::GrowthSystem::GrowableTree;

bool finished_growing(const GrowableTree& tree, int num_internodes) {
  return num_internodes >= tree.max_num_internodes ||
         num_internodes == tree.last_num_internodes;
}

void update_trees_finished_growing(Context* context) {
  std::vector<int> erase_at;
  int i{};
  for (auto& tree : context->trees) {
    const auto num_internodes = int(tree.nodes->internodes.size());
    if (finished_growing(tree, num_internodes)) {
      erase_at.push_back(i);
    }
    tree.last_num_internodes = num_internodes;
    i++;
  }

  erase_set(context->trees, erase_at);
}

void initialize_growth_cycle(Context* context) {
  context->environment_input.clear();
  context->sense_context.clear();
}

void growth_cycle(Context* context) {
  for (auto& tree : context->trees) {
    const tree::TreeID id = tree.nodes->id;
    for (auto& bud : tree.nodes->buds) {
      consume_within_occupancy_zone(id, bud, *context->attraction_points);
    }
  }

  for (auto& tree : context->trees) {
    for (auto& bud : tree.nodes->buds) {
      sense_bud(bud, *context->attraction_points, context->sense_context);
    }
  }

  context->environment_input = compute_environment_input(
    context->sense_context.closest_points_to_buds);

  for (auto& tree : context->trees) {
    apply_environment_input(*tree.nodes, context->environment_input, *tree.bud_q_params);
  }

  for (auto& tree : context->trees) {
    bud_fate(*tree.nodes, context->environment_input, *tree.spawn_params);
  }

  for (auto& tree : context->trees) {
    set_render_position(tree.nodes->internodes, 0);
  }

  update_trees_finished_growing(context);
}

void insert_attraction_points(Context* context) {
  for (auto& tree : context->trees) {
    auto pts = (*tree.make_attraction_points)();
    const uint32_t id = tree.nodes->id.id;
    for (auto& p : pts) {
      context->attraction_points->insert(p, tree::make_attraction_point(p, id));
    }
  }
}

void start_growing(Context* context) {
  context->stopwatch.reset();
  insert_attraction_points(context);
}

void finish_growing(Context* context) {
  context->growth_time = context->stopwatch.delta().count();
}

void grow(Context* context) {
  start_growing(context);

  while (!context->trees.empty()) {
    initialize_growth_cycle(context);
    growth_cycle(context);
  }

  finish_growing(context);
}

void worker(tree::GrowthSystem* system) {
  while (system->worker_keep_processing()) {
    if (system->worker_start_growing()) {
      grow(&system->context);
      system->fence.signal();
    }
    std::this_thread::sleep_for(sleep_dur);
  }
}

} //  anon

bool tree::GrowthSystem::worker_keep_processing() const {
  return keep_processing.load();
}

bool tree::GrowthSystem::worker_start_growing() {
  bool maybe_start{true};
  return start_growing.compare_exchange_strong(maybe_start, false);
}

void tree::GrowthSystem::fill_context(AttractionPoints* attraction_points,
                                      std::vector<GrowableTree>&& growable_trees) {
  assert(is_idle());
  context.trees = std::move(growable_trees);
  context.attraction_points = attraction_points;
}

void tree::GrowthSystem::submit() {
  assert(is_idle() && fence.is_ready() && !start_growing.load());
  state = State::Growing;
  fence.reset();
  start_growing.store(true);
}

bool tree::GrowthSystem::is_idle() const {
  return state == State::Idle;
}

tree::GrowthSystem::UpdateResult tree::GrowthSystem::update() {
  UpdateResult result{};
  if (state == State::Growing) {
    if (fence.is_ready()) {
      result.finished_growing = true;
      result.growth_time = context.growth_time;
      state = State::Idle;
    }
  }
  return result;
}

void tree::GrowthSystem::initialize() {
  assert(!work_thread.joinable() && !keep_processing.load());
  keep_processing.store(true);
  work_thread = std::thread{[this]() {
    worker(this);
  }};
}

void tree::GrowthSystem::terminate() {
  if (work_thread.joinable()) {
    keep_processing.store(false);
    work_thread.join();
  }
}

tree::GrowthSystem::~GrowthSystem() {
  terminate();
}

GROVE_NAMESPACE_END
