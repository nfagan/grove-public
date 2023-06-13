#include "WeatherGUI.hpp"
#include "../environment/WeatherComponent.hpp"
#include "grove/common/common.hpp"
#include <imgui/imgui.h>

GROVE_NAMESPACE_BEGIN

WeatherGUIUpdateResult WeatherGUI::render(WeatherComponent& component) {
  WeatherGUIUpdateResult result{};

  ImGui::Begin("WeatherGUI");

  const auto& status = component.weather_system.get_status();
  const char* curr_state_str = weather::to_string(status.current);
  const char* next_state_str = weather::to_string(status.next);
  ImGui::Text("Current %s, Next %s", curr_state_str, next_state_str);
  ImGui::Text("FracNext %0.3f", status.frac_next);

  bool update_enabled = component.weather_system.get_update_enabled();
  if (ImGui::Checkbox("UpdateEnabled", &update_enabled)) {
    result.update_enabled = update_enabled;
  }

  if (ImGui::Button("SetSunny")) {
    result.set_sunny = true;
  }

  if (ImGui::Button("SetOvercast")) {
    result.set_overcast = true;
  }

  if (ImGui::Button("Transition")) {
    result.immediate_transition = true;
  }

  auto stationary_t = float(component.weather_system.get_stationary_time());
  if (ImGui::SliderFloat("StationaryTime", &stationary_t, 1.0f, 240.0f)) {
    component.weather_system.set_stationary_time(stationary_t);
  }

  float frac_next = status.frac_next;
  if (ImGui::SliderFloat("FracNextState", &frac_next, 0.0f, 1.0f)) {
    result.set_frac_next = frac_next;
  }

  float rain_alpha_scale = component.params.rain_particle_alpha_scale;
  if (ImGui::SliderFloat("RainAlphaScale", &rain_alpha_scale, 0.0f, 1.0f)) {
    result.rain_alpha_scale = rain_alpha_scale;
  }

  float manual_rain_alpha_scale = component.params.manual_rain_particle_alpha_scale;
  if (ImGui::SliderFloat("ManualRainAlphaScale", &manual_rain_alpha_scale, 0.0f, 1.0f)) {
    result.manual_rain_alpha_scale = manual_rain_alpha_scale;
  }

  if (ImGui::Button("Close")) {
    result.close = true;
  }

  ImGui::End();
  return result;
}

GROVE_NAMESPACE_END
