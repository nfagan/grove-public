#include "IMGUIComponent.hpp"
#include "grove/common/common.hpp"
#include <imgui/imgui.h>

GROVE_NAMESPACE_BEGIN

void IMGUIComponent::render() {
  ImGui::Begin("AppGUI");
  ImGui::Checkbox("Enabled", &enabled);
  ImGui::Checkbox("GraphicsGUIEnabled", &graphics_gui_enabled);
  ImGui::Checkbox("AudioGUIEnabled", &audio_gui_enabled);
  ImGui::Checkbox("ProfileGUIEnabled", &profile_component_gui_enabled);
  ImGui::Checkbox("ProceduralTreeGUIEnabled", &procedural_tree_gui_enabled);
  ImGui::Checkbox("ProceduralTreeRootsGUIEnabled", &procedural_tree_roots_gui_enabled);
  ImGui::Checkbox("ProceduralFlowerGUIEnabled", &procedural_flower_gui_enabled);
  ImGui::Checkbox("WeatherGUIEnabled", &weather_gui_enabled);
  ImGui::Checkbox("EditorGUIEnabled", &editor_gui_enabled);
  ImGui::Checkbox("InputGUIEnabled", &input_gui_enabled);
  ImGui::Checkbox("SoilGUIEnabled", &soil_gui_enabled);
  ImGui::Checkbox("FogGUIEnabled", &fog_gui_enabled);
  ImGui::Checkbox("ArchGUIEnabled", &arch_gui_enabled);
  ImGui::Checkbox("SystemsGUIEnabled", &systems_gui_enabled);
  ImGui::Checkbox("SkyGUIEnabled", &sky_gui_enabled);
  ImGui::Checkbox("TerrainGUIEnabled", &terrain_gui_enabled);
  ImGui::Checkbox("SeasonGUIEnabled", &season_gui_enabled);
  ImGui::Checkbox("ParticleGUIEnabled", &particle_gui_enabled);
  ImGui::End();
}

GROVE_NAMESPACE_END
