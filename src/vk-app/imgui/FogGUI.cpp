#include "FogGUI.hpp"
#include "../cloud/FogComponent.hpp"
#include "grove/common/common.hpp"
#include <imgui/imgui.h>

GROVE_NAMESPACE_BEGIN

namespace {

void render_billboard(const FogComponent& component, FogGUIUpdateResult& result) {
  bool depth_test_enabled = component.debug_billboard_params.depth_test_enabled;
  if (ImGui::Checkbox("DepthTestEnabled", &depth_test_enabled)) {
    result.billboard_depth_test_enabled = depth_test_enabled;
  }

  const auto enter_flag = ImGuiInputTextFlags_EnterReturnsTrue;
  auto source_trs = component.billboard_transform->get_source();
  if (ImGui::InputFloat3("Translation", &source_trs.translation.x, "%0.3f", enter_flag)) {
    result.billboard_transform_source = source_trs;
  }
  if (ImGui::InputFloat3("Scale", &source_trs.scale.x, "%0.3f", enter_flag)) {
    result.billboard_transform_source = source_trs;
  }

  float opacity_scale = component.debug_billboard_params.opacity_scale;
  if (ImGui::SliderFloat("OpacityScale", &opacity_scale, 0.0f, 1.0f)) {
    result.billboard_opacity_scale = opacity_scale;
  }
}

void render_volume(const FogComponent& component, FogGUIUpdateResult& result) {
  if (!component.awaiting_noise_result) {
    if (ImGui::Button("RegenerateNoise")) {
      result.recompute_noise = true;
    }
  }

  const auto enter_flag = ImGuiInputTextFlags_EnterReturnsTrue;
  auto source_trs = component.debug_transform->get_source();
  if (ImGui::InputFloat3("Translation", &source_trs.translation.x, "%0.3f", enter_flag)) {
    result.new_transform_source = source_trs;
  }
  if (ImGui::InputFloat3("Scale", &source_trs.scale.x, "%0.3f", enter_flag)) {
    result.new_transform_source = source_trs;
  }

  if (ImGui::Button("MakeFog")) {
    result.make_fog = true;
  }

  bool depth_test_enable = component.debug_drawable_params.depth_test_enable;
  if (ImGui::Checkbox("DepthTestEnabled", &depth_test_enable)) {
    result.depth_test_enabled = depth_test_enable;
  }

  auto uvw_scale = component.debug_drawable_params.uvw_scale;
  if (ImGui::SliderFloat3("UVWScale", &uvw_scale.x, 0.0f, 4.0f)) {
    result.uvw_scale = uvw_scale;
  }

  auto uvw_off = component.debug_drawable_params.uvw_offset;
  if (ImGui::SliderFloat3("UVWOffset", &uvw_off.x, 0.0f, 1.0f)) {
    result.uvw_offset = uvw_off;
  }

  auto color = component.fog_color;
  if (ImGui::SliderFloat3("Color", &color.x, 0.0f, 1.0f)) {
    result.color = color;
  }

  float density_scale = component.debug_drawable_params.density_scale;
  if (ImGui::SliderFloat("Density", &density_scale, 0.0f, 4.0f)) {
    result.density = density_scale;
  }
  for (float th : {0.0f, 0.5f, 1.0f, 2.0f, 4.0f}) {
    char txt[64];
    if (int ct = std::snprintf(txt, 64, "D%0.3f", th); ct > 0 && ct < 64) {
      if (ImGui::SmallButton(txt)) {
        result.density = th;
      }
    }
  }

  bool manual_density_scale = component.manual_density_scale;
  if (ImGui::Checkbox("ManualDensityEnabled", &manual_density_scale)) {
    result.manual_density = manual_density_scale;
  }

  bool wind_influence_enabled = component.wind_influence_enabled;
  if (ImGui::Checkbox("WindInfluenceEnabled", &wind_influence_enabled)) {
    result.wind_influence_enabled = wind_influence_enabled;
  }

  float wind_influence_scale = component.wind_influence_scale;
  if (ImGui::SliderFloat("WindInfluenceScale", &wind_influence_scale, 0.0f, 1.0f)) {
    result.wind_influence_scale = wind_influence_scale;
  }
  if (ImGui::SmallButton("Wind0.25")) {
    result.wind_influence_scale = 0.25f;
  }
}

} //  anon

FogGUIUpdateResult FogGUI::render(const FogComponent& component) {
  FogGUIUpdateResult result;
  ImGui::Begin("FogGUI");

  if (ImGui::TreeNode("Volume")) {
    render_volume(component, result);
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("Billboard")) {
    render_billboard(component, result);
    ImGui::TreePop();
  }

  if (ImGui::Button("Close")) {
    result.close = true;
  }

  ImGui::End();
  return result;
}

GROVE_NAMESPACE_END
