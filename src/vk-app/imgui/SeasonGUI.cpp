#include "SeasonGUI.hpp"
#include "../environment/SeasonComponent.hpp"
#include "grove/common/common.hpp"
#include <imgui/imgui.h>

GROVE_NAMESPACE_BEGIN

SeasonGUIUpdateResult SeasonGUI::render(SeasonComponent& component) {
  SeasonGUIUpdateResult result{};

  ImGui::Begin("SeasonGUI");

  auto status = get_current_season_status(&component);
  ImGui::Text("CurrentState: %s", to_string(status.current));
  ImGui::Text("NextState: %s", to_string(status.next));
  ImGui::Text("FracNext: %0.3f", status.frac_next);

  auto* params = get_season_component_params(&component);
  ImGui::Checkbox("UpdateEnabled", &params->update_enabled);

  if (ImGui::Button("Summer")) {
    params->immediate_set_next = season::Season::Summer;
  }
  if (ImGui::Button("Fall")) {
    params->immediate_set_next = season::Season::Fall;
  }

  if (ImGui::Button("Close")) {
    result.close = true;
  }

  ImGui::End();

  return result;
}

GROVE_NAMESPACE_END
