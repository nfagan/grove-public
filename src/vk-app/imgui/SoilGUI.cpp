#include "SoilGUI.hpp"
#include "../terrain/SoilComponent.hpp"
#include "../terrain/soil_parameter_modulator.hpp"
#include "../generative/slime_mold.hpp"
#include "grove/common/common.hpp"
#include <imgui/imgui.h>

GROVE_NAMESPACE_BEGIN

SoilGUIUpdateResult SoilGUI::render(const SoilComponent& component,
                                    const soil::ParameterModulator& soil_param_modulator) {
  SoilGUIUpdateResult result;

  ImGui::Begin("SoilGUI");

  bool enabled = component.params.enabled;
  if (ImGui::Checkbox("Enabled", &enabled)) {
    result.enabled = enabled;
  }

  bool param_capture_enabled = soil_param_modulator.enabled;
  if (ImGui::Checkbox("ParameterCaptureEnabled", &param_capture_enabled)) {
    result.parameter_capture_enabled = param_capture_enabled;
  }

  if (ImGui::TreeNode("ParameterTargets")) {
    for (auto& param : soil_param_modulator.targets) {
      ImGui::Text("Parameter: %s", param.name);
    }
    ImGui::TreePop();
  }

  bool lock_param_targets = soil_param_modulator.lock_targets;
  if (ImGui::Checkbox("LockParameterTargets", &lock_param_targets)) {
    result.lock_parameter_targets = lock_param_targets;
  }

  bool draw_texture = component.params.draw_debug_image;
  if (ImGui::Checkbox("DrawTexture", &draw_texture)) {
    result.draw_texture = draw_texture;
  }

  bool overlay_player_pos = component.params.overlay_player_position;
  if (ImGui::Checkbox("OverlayPlayerPosition", &overlay_player_pos)) {
    result.overlay_player_position = overlay_player_pos;
  }

  float overlay_radius = component.params.overlay_radius;
  if (ImGui::SliderFloat("OverlayRadius", &overlay_radius, 0.0f, 16.0f)) {
    result.overlay_radius = overlay_radius;
  }

  const auto& soil_config = *component.get_soil()->read_config();
  auto decay = soil_config.decay;
  if (ImGui::SliderFloat("Decay", &decay, 0.001f, 0.5f)) {
    result.decay = decay;
  }

  auto ds = soil_config.diffuse_speed;
  if (ImGui::SliderFloat("DiffuseSpeed", &ds, 0.01f, 1.0f)) {
    result.diffuse_speed = ds;
  }

  bool diff_enabled = soil_config.diffuse_enabled;
  if (ImGui::Checkbox("DiffuseEnabled", &diff_enabled)) {
    result.diffuse_enabled = diff_enabled;
  }

  bool allow_perturb = soil_config.allow_perturb_event;
  if (ImGui::Checkbox("AllowPerturbEvent", &allow_perturb)) {
    result.allow_perturb_event = allow_perturb;
  }

  auto ts = soil_config.time_scale;
  if (ImGui::SliderFloat("TimeScale", &ts, 0.01f, 8.0f)) {
    result.time_scale = ts;
  }

  bool circ_world = soil_config.circular_world;
  if (ImGui::Checkbox("CircularWorld", &circ_world)) {
    result.circular_world = circ_world;
  }

  bool only_right_turns = soil_config.only_right_turns;
  if (ImGui::Checkbox("OnlyRightTurns", &only_right_turns)) {
    result.only_right_turns = only_right_turns;
  }

  ImGui::Text("TS Power %d", soil_config.turn_speed_power);
  if (ImGui::SmallButton("ScaleTurnSpeed2")) {
    result.turn_speed_power = soil_config.turn_speed_power + 1;
  }
  ImGui::SameLine();
  if (ImGui::SmallButton("ScaleTurnSpeed0.5")) {
    result.turn_speed_power = soil_config.turn_speed_power - 1;
  }

  ImGui::Text("Speed Power %d", soil_config.scale_speed_power);
  if (ImGui::SmallButton("ScaleSpeed2")) {
    result.speed_power = soil_config.scale_speed_power + 1;
  }
  ImGui::SameLine();
  if (ImGui::SmallButton("ScaleSpeed0.5")) {
    result.speed_power = soil_config.scale_speed_power - 1;
  }

  if (ImGui::Button("Close")) {
    result.close = true;
  }

  ImGui::End();
  return result;
}

GROVE_NAMESPACE_END
