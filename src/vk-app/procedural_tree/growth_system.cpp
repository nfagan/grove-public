#include "growth_system.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace tree;
using GrowthSystem = GrowthSystem2;
using Instance = GrowthSystem::Instance;
using FutureGrowthResult = GrowthSystem::FutureGrowthResult;
using AsyncState = GrowthSystem2::AsyncState;

template <typename Vec, typename Inst>
Inst* find_if_impl(Vec* vec, uint32_t id) {
  auto it = std::find_if(vec->begin(), vec->end(), [id](const auto& inst) {
    return inst->id == id;
  });
  return it == vec->end() ? nullptr : it->get();
}

GrowthSystem::GrowthContext* find_growth_context(GrowthSystem* sys, GrowthContextHandle handle) {
  using Vec = std::vector<std::unique_ptr<GrowthSystem::GrowthContext>>;
  return find_if_impl<Vec, GrowthSystem::GrowthContext>(&sys->growth_contexts, handle.id);
}

const GrowthSystem::GrowthContext* find_growth_context(const GrowthSystem* sys,
                                                      GrowthContextHandle handle) {
  using Vec = const std::vector<std::unique_ptr<GrowthSystem::GrowthContext>>;
  return find_if_impl<Vec, const GrowthSystem::GrowthContext>(&sys->growth_contexts, handle.id);
}

bool can_read_async_modifiable(const GrowthSystem::GrowthContext& ctx) {
  return ctx.async_state == AsyncState::Idle;
}

bool async_complete(const GrowthSystem::GrowthContext& ctx) {
  return ctx.async_finished.load();
}

bool is_async_idle(const GrowthSystem::GrowthContext& ctx) {
  return ctx.async_state == AsyncState::Idle;
}

std::unordered_set<uint32_t> tree_ids_to_uint32_set(const std::vector<tree::TreeID>& ids) {
  std::unordered_set<uint32_t> res;
  for (const tree::TreeID id : ids) {
    res.insert(id.id);
  }
  return res;
}

size_t deactivate_attraction_points(AttractionPoints* oct,
                                    const std::unordered_set<uint32_t>& ids) {
  return oct->clear_if([&ids](const AttractionPoint* data) {
    return ids.count(data->id());
  });
}

void rebuild_attraction_points(AttractionPoints* oct) {
  *oct = AttractionPoints::rebuild_active(std::move(*oct));
}

[[maybe_unused]] void validate_rebuild(const AttractionPoints* oct, size_t orig_num_active) {
  oct->validate();
  assert(oct->count_non_empty_leaves() == orig_num_active);
  assert(oct->count_empty_leaves() == 0);
  (void) oct;
  (void) orig_num_active;
}

void deactivate_rebuild_attraction_points(GrowthSystem::GrowthContext* ctx,
                                          const std::vector<TreeID>& ids) {
  assert(!ids.empty());
  auto* oct = &ctx->attraction_points;

#ifdef GROVE_DEBUG
  size_t num_active = oct->count_non_empty_leaves();
#endif

  size_t num_cleared = deactivate_attraction_points(oct, tree_ids_to_uint32_set(ids));
  rebuild_attraction_points(oct);

#ifdef GROVE_DEBUG
  assert(num_cleared <= num_active);
  validate_rebuild(oct, num_active - num_cleared);
#else
  (void) num_cleared;
#endif
}

std::unique_ptr<Instance> make_instance(PrepareToGrowParams&& params, FutureGrowthResult fut) {
  assert(params.make_attraction_points);
  auto result = std::make_unique<Instance>();
  result->context_handle = params.context;
  result->nodes = std::move(params.nodes);
  result->spawn_params = std::move(params.spawn_params);
  result->bud_q_params = std::move(params.bud_q_params);
  result->make_attraction_points = std::move(params.make_attraction_points);
  result->max_num_internodes = params.max_num_internodes;
  result->result = std::move(fut);
  return result;
}

GrowthSystem::NodeGrowthResult make_result_data(Instance&& inst) {
  GrowthSystem::NodeGrowthResult result;
  result.nodes = std::move(inst.nodes);
  result.context_handle = inst.context_handle;
  result.spawn_params = std::move(inst.spawn_params);
  result.bud_q_params = std::move(inst.bud_q_params);
  result.make_attraction_points = std::move(inst.make_attraction_points);
  return result;
}

std::unique_ptr<GrowthSystem::GrowthContext>
make_system_growth_context(uint32_t id, const CreateGrowthContextParams& params) {
  const int points_buff_size = params.max_num_attraction_points_per_tree;
  const float init_span_size = params.initial_attraction_point_span_size;
  const float max_span_size_split = params.max_attraction_point_span_size_split;
  assert(points_buff_size > 0 && init_span_size > 0.0f && max_span_size_split > 0.0f);

  auto result = std::make_unique<GrowthSystem::GrowthContext>();
  result->id = id;
  result->attraction_points_buffer = std::make_unique<Vec3f[]>(points_buff_size);
  result->attraction_points_buffer_size = points_buff_size;
  result->attraction_points = AttractionPoints{init_span_size, max_span_size_split};
  result->async_finished.store(true);
  return result;
}

tree::GrowthContext to_tree_growth_context(GrowthSystem::GrowthContext* ctx) {
  tree::GrowthContext result{};
  result.trees = make_data_array_view<GrowableTree>(ctx->growable_trees);
  result.attraction_points_buffer = ArrayView<Vec3f>{
    ctx->attraction_points_buffer.get(),
    ctx->attraction_points_buffer.get() + ctx->attraction_points_buffer_size};
  result.environment_input = &ctx->environment_input;
  result.attraction_points = &ctx->attraction_points;
  result.sense_context = &ctx->sense_context;
  return result;
}

tree::GrowableTree to_growable_tree(Instance* inst) {
  return make_growable_tree(
    &inst->nodes,
    &inst->spawn_params,
    &inst->bud_q_params,
    &inst->make_attraction_points,
    inst->max_num_internodes);
}

[[maybe_unused]] void validate_start_growing(GrowthSystem::GrowthContext* context) {
  assert(is_async_idle(*context) &&
         context->awaiting_growth &&
         async_complete(*context) &&
         context->growable_trees.empty() &&
         context->growing_instances.empty());
  (void) context;
}

void start_growing(GrowthSystem* sys, GrowthSystem::GrowthContext* context) {
  validate_start_growing(context);

  context->async_state = AsyncState::Growing;
  context->awaiting_growth = false;
  context->async_finished.store(false);

  auto inst_it = sys->instances.begin();
  while (inst_it != sys->instances.end()) {
    Instance& instance = *inst_it->get();
    if (!instance.growing && instance.context_handle.id == context->id) {
      instance.growing = true;
      context->growable_trees.push_back(to_growable_tree(&instance));
      context->growing_instances.push_back(std::move(*inst_it));
      inst_it = sys->instances.erase(inst_it);
    } else {
      ++inst_it;
    }
  }

  context->async_future = std::async(std::launch::async, [context]() {
    auto tree_context = to_tree_growth_context(context);
    context->growth_result = tree::grow(&tree_context);
    context->async_finished.store(true);
  });
}

void start_clearing_attraction_points(GrowthSystem*, GrowthSystem::GrowthContext* ctx) {
  assert(is_async_idle(*ctx) && async_complete(*ctx));

  ctx->async_state = AsyncState::ClearingAttractionPoints;
  ctx->async_finished.store(false);
  auto task = [ctx, ids = std::move(ctx->pending_clear_attraction_points)]() {
    deactivate_rebuild_attraction_points(ctx, ids);
    ctx->async_finished.store(true);
  };

  ctx->async_future = std::async(std::launch::async, std::move(task));
  ctx->pending_clear_attraction_points.clear();
}

void on_async_finish(GrowthSystem::GrowthContext* context) {
  context->async_future.get(); //  should not block.
  context->async_state = AsyncState::Idle;
}

bool check_finished_growing(GrowthSystem*, GrowthSystem::GrowthContext* context) {
  if (async_complete(*context)) {
    on_async_finish(context);
    context->growable_trees.clear();
    for (std::unique_ptr<Instance>& inst : context->growing_instances) {
      inst->result->data = make_result_data(std::move(*inst));
      inst->result->mark_ready();
    }
    context->growing_instances.clear();
    return true;
  } else {
    return false;
  }
}

bool check_finished_clearing_attraction_points(GrowthSystem*, GrowthSystem::GrowthContext* ctx) {
  if (async_complete(*ctx)) {
    on_async_finish(ctx);
    return true;
  } else {
    return false;
  }
}

} //  anon

GrowthContextHandle tree::create_growth_context(GrowthSystem* sys,
                                                const CreateGrowthContextParams& params) {
  uint32_t id{sys->next_growth_context_id++};
  sys->growth_contexts.emplace_back() = make_system_growth_context(id, params);
  GrowthContextHandle handle{id};
  return handle;
}

GrowthSystem2::ReadGrowthContext tree::read_growth_context(const GrowthSystem2* sys,
                                                           GrowthContextHandle handle) {
  GrowthSystem2::ReadGrowthContext result{};
  if (auto* ctx = find_growth_context(sys, handle)) {
    result.events = ctx->events;
    if (can_read_async_modifiable(*ctx)) {
      result.growth_result = &ctx->growth_result;
      result.attraction_points = &ctx->attraction_points;
    }
  } else {
    assert(false);
  }
  return result;
}

void tree::grow(GrowthSystem* sys, GrowthContextHandle handle) {
  auto* ctx = find_growth_context(sys, handle);
  assert(ctx);
  assert(ctx->async_state != AsyncState::Growing && "Already growing.");
  ctx->awaiting_growth = true;
}

bool tree::can_grow(const GrowthSystem* sys, GrowthContextHandle handle) {
  const auto* ctx = find_growth_context(sys, handle);
  assert(ctx);
  return ctx->async_state != AsyncState::Growing;
}

FutureGrowthResult tree::prepare_to_grow(GrowthSystem* sys, PrepareToGrowParams&& params) {
  assert(params.context.id && find_growth_context(sys, params.context));
  auto fut = std::make_shared<Future<GrowthSystem::NodeGrowthResult>>();
  sys->instances.emplace_back() = make_instance(std::move(params), fut);
  return fut;
}

void tree::push_pending_attraction_points_clear(GrowthSystem2* sys, GrowthContextHandle handle,
                                                tree::TreeID id) {
  auto* ctx = find_growth_context(sys, handle);
  assert(ctx);
  ctx->pending_clear_attraction_points.push_back(id);
}

void tree::update(GrowthSystem* sys) {
  for (auto& context : sys->growth_contexts) {
    context->events = {};
  }

  for (auto& context : sys->growth_contexts) {
    if (is_async_idle(*context) && context->awaiting_growth) {
      start_growing(sys, context.get());
    }
    if (is_async_idle(*context) && !context->pending_clear_attraction_points.empty()) {
      start_clearing_attraction_points(sys, context.get());
    }
  }

  for (auto& context : sys->growth_contexts) {
    if (context->async_state == AsyncState::Growing) {
      if (check_finished_growing(sys, context.get())) {
        context->events.just_finished_growing = true;
      }
    } else if (context->async_state == AsyncState::ClearingAttractionPoints) {
      if (check_finished_clearing_attraction_points(sys, context.get())) {
        context->events.just_finished_clearing_attraction_points = true;
      }
    }
  }
}

GROVE_NAMESPACE_END
