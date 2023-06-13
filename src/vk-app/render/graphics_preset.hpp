#pragma once

namespace grove {
class RenderComponent;
}

namespace grove::tree {
struct RenderTreeSystem;
}

namespace grove::vk {
struct GraphicsContext;
}

namespace grove::gfx {

struct Context;

enum class QualityPreset {
  Normal = 0,
  Low = 1,
};

struct QualityPresetSystem;

struct QualityPresetUpdateInfo {
  RenderComponent& render_component;
  vk::GraphicsContext& vk_context;
  Context& gfx_context;
  tree::RenderTreeSystem& render_tree_system;
};

QualityPresetSystem* get_global_quality_preset_system();

QualityPreset get_current_quality_preset(const QualityPresetSystem* sys);
void set_quality_preset(QualityPresetSystem* sys, QualityPreset preset);

bool get_set_feature_volumetrics_disabled(QualityPresetSystem* sys, const bool* v);

void update_quality_preset_system(QualityPresetSystem* sys, const QualityPresetUpdateInfo& info);

}