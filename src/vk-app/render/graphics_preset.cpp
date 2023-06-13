#include "graphics_preset.hpp"
#include "RenderComponent.hpp"
#include "render_branch_nodes_gpu.hpp"
#include "render_tree_leaves_gpu.hpp"
#include "../procedural_tree/render_tree_system.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Optional.hpp"

GROVE_NAMESPACE_BEGIN

namespace gfx {

struct QualityPresetSystem {
  QualityPreset curr_preset{QualityPreset::Normal};
  Optional<QualityPreset> target_preset;
  bool curr_volumetrics_disabled{};
  Optional<bool> set_volumetrics_disabled;
};

} //  gfx

using namespace gfx;

namespace {

void apply_low_preset(const QualityPresetUpdateInfo& info) {
  //  Branch nodes.
  bool shadow_disabled{true};
  tree::get_set_render_branch_nodes_wind_shadow_disabled(&shadow_disabled);
  tree::get_set_render_branch_nodes_base_shadow_disabled(&shadow_disabled);

  //  Tree leaves.
  bool pcf_disabled{true};
  foliage::get_set_tree_leaves_renderer_pcf_disabled(&pcf_disabled);
  tree::maybe_set_preferred_foliage_lod(&info.render_tree_system, 1);
}

void apply_normal_preset(const QualityPresetUpdateInfo& info) {
  //  Branch nodes.
  bool shadow_disabled{};
  tree::get_set_render_branch_nodes_wind_shadow_disabled(&shadow_disabled);
  tree::get_set_render_branch_nodes_base_shadow_disabled(&shadow_disabled);

  //  Tree leaves.
  bool pcf_disabled{};
  foliage::get_set_tree_leaves_renderer_pcf_disabled(&pcf_disabled);
  tree::maybe_set_preferred_foliage_lod(&info.render_tree_system, 0);
}

struct {
  QualityPresetSystem sys;
} globals;

} //  anon

QualityPresetSystem* gfx::get_global_quality_preset_system() {
  return &globals.sys;
}

void gfx::set_quality_preset(QualityPresetSystem* sys, QualityPreset preset) {
  if (preset != sys->curr_preset) {
    sys->target_preset = preset;
  }
}

QualityPreset gfx::get_current_quality_preset(const QualityPresetSystem* sys) {
  return sys->curr_preset;
}

bool gfx::get_set_feature_volumetrics_disabled(QualityPresetSystem* sys, const bool* v) {
  if (v && *v != sys->curr_volumetrics_disabled) {
    sys->set_volumetrics_disabled = *v;
  }
  return sys->curr_volumetrics_disabled;
}

void gfx::update_quality_preset_system(
  QualityPresetSystem* sys, const QualityPresetUpdateInfo& info) {
  //
  if (sys->target_preset) {
    assert(sys->target_preset.value() != sys->curr_preset);
    QualityPreset targ_preset = sys->target_preset.value();
    sys->target_preset = NullOpt{};
    sys->curr_preset = targ_preset;

    switch (targ_preset) {
      case QualityPreset::Low: {
        apply_low_preset(info);
        break;
      }
      case QualityPreset::Normal: {
        apply_normal_preset(info);
        break;
      }
    }
  }

  if (sys->set_volumetrics_disabled) {
    bool disable = sys->set_volumetrics_disabled.value();
    sys->set_volumetrics_disabled = NullOpt{};
    sys->curr_volumetrics_disabled = disable;
    info.render_component.cloud_renderer.set_volume_enabled(!disable);
  }
}

GROVE_NAMESPACE_END
