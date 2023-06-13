#include "SkyGUI.hpp"
#include "../sky/SkyComponent.hpp"
#include "grove/math/constants.hpp"
#include "grove/common/common.hpp"
#include <imgui/imgui.h>

GROVE_NAMESPACE_BEGIN

SkyGUIUpdateResult SkyGUI::render(const SkyComponent& component) {
  SkyGUIUpdateResult result{};
  ImGui::Begin("SkyGUI");

  bool weather_controls_sky_grad = component.weather_controls_sky_gradient;
  if (ImGui::Checkbox("WeatherControlsSkyGradient", &weather_controls_sky_grad)) {
    result.weather_controls_gradient = weather_controls_sky_grad;
  }

  bool use_sun_angles = component.use_sun_angles;
  if (ImGui::Checkbox("UseSunAngles", &use_sun_angles)) {
    result.use_sun_angles = use_sun_angles;
  }

  auto grad_params = component.gradient_params_from_user;
  Vec3f* grad_colors[4] = {
    &grad_params.y0_color,
    &grad_params.y1_color,
    &grad_params.y2_color,
    &grad_params.y3_color,
  };

  for (int i = 0; i < 4; i++) {
    std::string label_id = "GradientColor" + std::to_string(i);
    if (ImGui::SliderFloat3(label_id.c_str(), &grad_colors[i]->x, 0.0f, 1.0f)) {
      result.sky_gradient_params = grad_params;
    }
  }

  auto sun_position_theta01 = float(component.sun_position_theta_frac);
  auto sun_position_phi_radians = float(component.sun_position_phi_radians);

  if (ImGui::SliderFloat("SunPositionTheta01", &sun_position_theta01, 0.0f, 1.0f)) {
    result.sun_position_theta01 = sun_position_theta01;
  }
  if (ImGui::SliderFloat("SunPositionPhiRadians", &sun_position_phi_radians, 0.0f, 2.0f * pif())) {
    result.sun_position_phi_radians = sun_position_phi_radians;
  }

  if (ImGui::Button("UseDefaultSun")) {
    result.use_default_sun = true;
  }

  if (ImGui::Button("UseAltSun")) {
    result.use_sun_angles = true;
    result.sun_position_theta01 = 0.357f;
  }

  if (ImGui::Button("Close")) {
    result.close = true;
  }

  ImGui::End();
  return result;
}

GROVE_NAMESPACE_END
