#include "debug_growth_system.hpp"
#include "utility.hpp"
#include "grove/common/common.hpp"
#include <vector>

GROVE_NAMESPACE_BEGIN

namespace {

using namespace tree;
using namespace tree::debug;

struct DebugGrowthContext {
  GrowthContextHandle context_handle;
  Optional<vk::PointBufferRenderer::DrawableHandle> point_drawable;
  bool need_update_points_drawable;
  Optional<bool> activate_deactivate_point_drawable;
  bool is_point_drawable_active;
};

struct DebugGrowthContexts {
  std::vector<DebugGrowthContext> contexts;
} global_contexts;

void update_context(DebugGrowthContext& ctx, const DebugGrowthContextUpdateInfo& info) {
  auto inst = read_growth_context(info.growth_system, ctx.context_handle);
  if (inst.events.just_finished_growing ||
      inst.events.just_finished_clearing_attraction_points) {
    ctx.need_update_points_drawable = true;
  }

  if (ctx.need_update_points_drawable &&
      inst.attraction_points &&
      (ctx.activate_deactivate_point_drawable || ctx.is_point_drawable_active)) {
    auto ps = tree::extract_octree_points(*inst.attraction_points);
    if (!ctx.point_drawable) {
      vk::PointBufferRenderer::DrawableParams draw_params{};
      draw_params.color = Vec3f{1.0f};
      draw_params.point_size = 4.0f;
      ctx.point_drawable = info.pb_renderer.create_drawable(
        vk::PointBufferRenderer::DrawableType::Points,
        draw_params);
    }
    if (ctx.point_drawable) {
      info.pb_renderer.reserve_instances(
        info.renderer_context,
        ctx.point_drawable.value(),
        uint32_t(ps.size()));
      info.pb_renderer.set_instances(
        info.renderer_context,
        ctx.point_drawable.value(),
        ps.data(),
        int(ps.size()), 0);
      ctx.need_update_points_drawable = false;
    }
  }

  if (!ctx.need_update_points_drawable &&
      ctx.activate_deactivate_point_drawable &&
      ctx.point_drawable) {
    ctx.is_point_drawable_active = ctx.activate_deactivate_point_drawable.value();
    ctx.activate_deactivate_point_drawable = NullOpt{};
    if (ctx.is_point_drawable_active) {
      info.pb_renderer.add_active_drawable(ctx.point_drawable.value());
    } else {
      info.pb_renderer.remove_active_drawable(ctx.point_drawable.value());
    }
  }
}

} //  anon

void tree::debug::create_debug_growth_context_instance(GrowthContextHandle handle) {
  DebugGrowthContext ctx{};
  ctx.context_handle = handle;
  ctx.need_update_points_drawable = true;
  global_contexts.contexts.push_back(std::move(ctx));
}

void tree::debug::update_debug_growth_contexts(const DebugGrowthContextUpdateInfo& info) {
  for (auto& ctx : global_contexts.contexts) {
    update_context(ctx, info);
  }
}

bool tree::debug::is_debug_growth_context_point_drawable_active(GrowthContextHandle context) {
  for (auto& ctx : global_contexts.contexts) {
    if (ctx.context_handle == context) {
      return ctx.is_point_drawable_active;
    }
  }
  return false;
}

void tree::debug::set_debug_growth_context_point_drawable_active(GrowthContextHandle context,
                                                                 bool v) {
  for (auto& ctx : global_contexts.contexts) {
    if (ctx.context_handle == context) {
      ctx.activate_deactivate_point_drawable = v;
      return;
    }
  }
}

GROVE_NAMESPACE_END
