#include "ProfileComponent.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

void ProfileComponent::initialize() {
  profiler.add_active("App/render");
  profiler.add_active("App/update");
  profiler.add_active("App/begin_frame");
  profiler.add_active("App/forward_pass");
  profiler.add_active("App/shadow_pass");
  profiler.add_active("App/new_ui");
//  profiler.add_active("DebugTerrainComponent/move_sphere");
//  profiler.add_active("DebugTerrainComponent/update");
//  profiler.add_active("TreeSystem/prune_intersecting_radius_limiter");
  profiler.add_graphics_active("App/shadow_pass");
  profiler.add_graphics_active("App/forward_pass");
}

void ProfileComponent::update() {
  profiler.update();
}

void ProfileComponent::on_gui_update(const ProfileComponentGUI::UpdateResult& update_res) {
  if (update_res.add_profile) {
    profiler.add_active(update_res.add_profile.value());
  }
  if (update_res.remove_profile) {
    profiler.remove_active(update_res.remove_profile.value());
  }
  if (update_res.add_gfx_profile) {
    profiler.add_graphics_active(update_res.add_gfx_profile.value());
  }
  if (update_res.remove_gfx_profile) {
    profiler.remove_graphics_active(update_res.remove_gfx_profile.value());
  }
}

GROVE_NAMESPACE_END
