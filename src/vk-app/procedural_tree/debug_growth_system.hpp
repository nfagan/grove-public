#pragma once

#include "growth_system.hpp"
#include "../render/PointBufferRenderer.hpp"

namespace grove::tree::debug {

struct DebugGrowthContextUpdateInfo {
  const GrowthSystem2* growth_system;
  vk::PointBufferRenderer& pb_renderer;
  const vk::PointBufferRenderer::AddResourceContext& renderer_context;
};

void create_debug_growth_context_instance(GrowthContextHandle context);
void update_debug_growth_contexts(const DebugGrowthContextUpdateInfo& info);

bool is_debug_growth_context_point_drawable_active(GrowthContextHandle context);
void set_debug_growth_context_point_drawable_active(GrowthContextHandle context, bool v);

}