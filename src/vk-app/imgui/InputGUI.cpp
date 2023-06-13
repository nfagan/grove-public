#include "InputGUI.hpp"
#include "../camera/CameraComponent.hpp"
#include "grove/visual/Camera.hpp"
#include "grove/common/common.hpp"
#include "grove/input/Controller.hpp"
#include <imgui/imgui.h>
#include <algorithm>

GROVE_NAMESPACE_BEGIN

InputGUIUpdateResult InputGUI::render(
  CameraComponent& camera_component, Controller& controller, const Camera& camera) {
  //
  InputGUIUpdateResult result{};
  ImGui::Begin("InputGUI");

  auto cam_pos = camera.get_position();
  if (ImGui::InputFloat3("Position", &cam_pos.x, "%0.3f", ImGuiInputTextFlags_EnterReturnsTrue)) {
    result.set_position = cam_pos;
  }

  const auto& cam_params = camera_component.get_params();
  float fps_cam_height = cam_params.fps_height;
  if (ImGui::InputFloat("FPSCameraHeight", &fps_cam_height)) {
    result.fps_camera_height = fps_cam_height;
  }

  float move_speed = cam_params.move_speed;
  if (ImGui::InputFloat("MoveSpeed", &move_speed)) {
    result.move_speed = std::max(0.0f, move_speed);
  }

  if (ImGui::SmallButton("Slower")) {
    result.move_speed = std::max(0.0f, move_speed - 0.05f);
  }
  ImGui::SameLine();
  if (ImGui::SmallButton("Faster")) {
    result.move_speed = move_speed + 0.05f;
  }

  bool free_roam = cam_params.free_roaming;
  if (ImGui::Checkbox("LockCameraToGround", &free_roam)) {
    camera_component.set_free_roaming(free_roam);
  }

  auto sens = float(controller.get_rotation_sensitivity());
  if (ImGui::SliderFloat("Sensitivity", &sens, 0.0f, 1.0f)) {
    controller.set_rotation_sensitivity(sens);
  }

  auto smooth = float(controller.get_rotation_smoothing());
  if (ImGui::SliderFloat("Smoothing", &smooth, 0.0f, 1.0f)) {
    controller.set_rotation_smoothing(smooth);
  }

  if (ImGui::Button("Close")) {
    result.close = true;
  }

  ImGui::End();
  return result;
}

GROVE_NAMESPACE_END
