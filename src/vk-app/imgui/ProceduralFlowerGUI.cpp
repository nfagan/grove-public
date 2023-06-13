#include "ProceduralFlowerGUI.hpp"
#include "../procedural_flower/ProceduralFlowerComponent.hpp"
#include "grove/common/common.hpp"
#include <imgui/imgui.h>

GROVE_NAMESPACE_BEGIN

namespace {

constexpr auto enter_flag() {
  return ImGuiInputTextFlags_EnterReturnsTrue;
}

int num_ornaments(const ProceduralFlowerComponent& component) {
  int s{};
  for (auto& [_, flower] : component.flowers) {
    s += int(flower.ornaments.size());
  }
  return s;
}

bool render_color(const char* v, Vec3<uint8_t>* src) {
  int values[3] = {src->x, src->y, src->z};
  if (ImGui::InputInt3(v, values)) {
    Vec3<uint8_t> result{};
    for (int i = 0; i < 3; i++) {
      result[i] = uint8_t(clamp(values[i], 0, 255));
    }
    *src = result;
    return true;
  } else {
    return false;
  }
}

void render_ornament(const ProceduralFlowerComponent::Ornament& orn,
                     ProceduralFlowerGUI::UpdateResult& result) {
  bool has_colors = false;
  ProceduralFlowerGUI::SetColors4 colors;
  colors.c0 = orn.alpha_test_petal_material_params.color0;
  colors.c1 = orn.alpha_test_petal_material_params.color1;
  colors.c2 = orn.alpha_test_petal_material_params.color2;
  colors.c3 = orn.alpha_test_petal_material_params.color3;
  if (render_color("Color0", &colors.c0)) {
    has_colors = true;
  }
  if (render_color("Color1", &colors.c1)) {
    has_colors = true;
  }
  if (render_color("Color2", &colors.c2)) {
    has_colors = true;
  }
  if (render_color("Color3", &colors.c3)) {
    has_colors = true;
  }
  if (has_colors) {
    result.set_alpha_test_colors = colors;
  }
}

} //  anon

ProceduralFlowerGUI::UpdateResult
ProceduralFlowerGUI::render(const ProceduralFlowerComponent& component) {
  UpdateResult result{};
  ImGui::Begin("ProceduralFlowerGUI");

  ImGui::Text("NumStems: %d", int(component.stems.size()));
  ImGui::Text("NumFlowers: %d", int(component.flowers.size()));
  ImGui::Text("NumOrnaments: %d", num_ornaments(component));

  bool render_attrac_points = component.params.render_attraction_points;
  if (ImGui::Checkbox("RenderAttractionPoints", &render_attrac_points)) {
    result.render_attraction_points = render_attrac_points;
  }

  bool death_enabled = component.params.death_enabled;
  if (ImGui::Checkbox("DeathEnabled", &death_enabled)) {
    result.death_enabled = death_enabled;
  }

  ImGui::InputFloat2("PatchPosition", &patch_position.x, "%0.3f", enter_flag());
  if (ImGui::Button("AddPatch")) {
    result.add_patch = patch_position;
  }

  float patch_pos_radius = component.params.patch_position_radius;
  if (ImGui::InputFloat("PatchPositionRadius", &patch_pos_radius)) {
    result.patch_position_radius = patch_pos_radius;
  }

  float patch_radius = component.params.patch_radius;
  if (ImGui::InputFloat("PatchRadius", &patch_radius)) {
    result.patch_radius = patch_radius;
  }

  int patch_size = component.params.patch_size;
  if (ImGui::InputInt("PatchSize", &patch_size) && patch_size > 0) {
    result.patch_size = patch_size;
  }

  float flower_stem_scale = component.params.flower_stem_scale;
  if (ImGui::InputFloat("FlowerStemScale", &flower_stem_scale)) {
    result.flower_stem_scale = flower_stem_scale;
  }

  float flower_radius_power = component.params.flower_radius_power;
  if (ImGui::InputFloat("FlowerRadiusPower", &flower_radius_power)) {
    result.flower_radius_power = flower_radius_power;
  }

  float flower_radius_scale = component.params.flower_radius_scale;
  if (ImGui::InputFloat("FlowerRadiusScale", &flower_radius_scale)) {
    result.flower_radius_scale = flower_radius_scale;
  }

  float flower_radius_randomness = component.params.flower_radius_randomness;
  if (ImGui::SliderFloat("FlowerRadiusRandomness", &flower_radius_randomness, 0.0f, 1.0f)) {
    result.flower_radius_randomness = flower_radius_randomness;
  }

  float flower_radius_power_randomness = component.params.flower_radius_power_randomness;
  if (ImGui::SliderFloat("FlowerRadiusPowerRandomness", &flower_radius_power_randomness, 0.0f, 1.0f)) {
    result.flower_radius_power_randomness = flower_radius_power_randomness;
  }

  bool rand_radius_power = component.params.randomize_flower_radius_power;
  if (ImGui::Checkbox("RandomizeFlowerRadiusPower", &rand_radius_power)) {
    result.randomize_flower_radius_power = rand_radius_power;
  }

  bool rand_radius_scale = component.params.randomize_flower_radius_scale;
  if (ImGui::Checkbox("RandomizeFlowerRadiusScale", &rand_radius_scale)) {
    result.randomize_flower_radius_scale = rand_radius_scale;
  }

  float orn_growth_incr = component.params.ornament_growth_incr;
  if (ImGui::SliderFloat("OrnamentGrowthIncr", &orn_growth_incr, 0.0f, 0.2f)) {
    result.ornament_growth_incr = orn_growth_incr;
  }

  float axis_growth_incr = component.params.axis_growth_incr;
  if (ImGui::SliderFloat("AxisGrowthIncr", &axis_growth_incr, 0.0f, 1.0f)) {
    result.axis_growth_incr = axis_growth_incr;
  }

  bool allow_bush = component.params.allow_bush;
  if (ImGui::Checkbox("AllowBush", &allow_bush)) {
    result.allow_bush = allow_bush;
  }

  if (ImGui::Button("EnableRandomization")) {
    result.enable_randomization = true;
  }

  if (ImGui::TreeNode("ShowSelectable")) {
    for (auto& [id, flower] : component.flowers) {
      auto flower_str = std::string{"Flower"};

      if (component.selected_flower && component.selected_flower.value() == id) {
        flower_str = std::string{"(*)"} + flower_str;
      }

      flower_str += std::to_string(id.id);
      if (ImGui::SmallButton(flower_str.c_str())) {
        result.selected_flower = id.id;
      }

      if (component.selected_flower && id == component.selected_flower.value()) {
        int orn_ind{};
        for (auto& orn : flower.ornaments) {
          std::string orn_id{"Ornament"};
          orn_id += std::to_string(orn_ind);
          if (ImGui::TreeNode(orn_id.c_str())) {
            render_ornament(orn, result);
            ImGui::TreePop();
          }
          orn_ind++;
        }
      }
    }

    ImGui::TreePop();
  }

  if (ImGui::Button("Close")) {
    result.close = true;
  }

  ImGui::End();
  return result;
}

GROVE_NAMESPACE_END
