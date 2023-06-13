#include "ProceduralTreeGUI.hpp"
#include "../procedural_tree/debug_growth_system.hpp"
#include "../procedural_tree/ProceduralTreeComponent.hpp"
#include "../procedural_tree/utility.hpp"
#include "grove/common/common.hpp"
#include "grove/common/stats.hpp"
#include "grove/imgui/IMGUIWrapper.hpp"
#include <imgui/imgui.h>

GROVE_NAMESPACE_BEGIN

namespace {

using Trees = ProceduralTreeComponent::Trees;

DynamicArray<float, 32> scratch_space;

constexpr auto enter_flag() {
  return ImGuiInputTextFlags_EnterReturnsTrue;
}

[[maybe_unused]] void clear_reserve_scratch_space(std::size_t) {
  scratch_space.clear();
  //  scratch_space.reserve(size);
}
#if 0
void push_scratch_diameters(const Trees& trees) {
  clear_reserve_scratch_space(trees.size());
  for (auto& [id, tree] : trees) {
    if (!tree.nodes.internodes.empty()) {
      scratch_space.push_back(tree.nodes.internodes[0].diameter);
    }
  }
}

void push_num_internodes(const Trees& trees) {
  clear_reserve_scratch_space(trees.size());
  for (auto& [id, tree] : trees) {
    scratch_space.push_back(float(tree.nodes.internodes.size()));
  }
}

void push_num_leaves(const Trees& trees) {
  clear_reserve_scratch_space(trees.size());
  for (auto& [id, tree] : trees) {
    int num_leaves{};
    for (auto& node : tree.nodes.internodes) {
      if (node.is_leaf()) {
        num_leaves++;
      }
    }
    scratch_space.push_back(float(num_leaves));
  }
}
#endif

[[maybe_unused]] float mean_scratch() {
  return stats::mean_or_default(scratch_space.data(), scratch_space.size(), 0.0f);
}

[[maybe_unused]] float std_scratch() {
  return stats::std_or_default(scratch_space.data(), scratch_space.size(), 0.0f);
}

int num_growable_trees(const Trees& trees) {
  int ct{};
  for (auto& [id, tree] : trees) {
    ct += !tree.meta.finished_growing;
  }
  return ct;
}

} //  anon

ProceduralTreeGUI::GUIUpdateResult
ProceduralTreeGUI::render(ProceduralTreeComponent& component, const tree::GrowthSystem2* growth_system) {
  GUIUpdateResult result{};

  ImGui::Begin("ProceduralTreeGUI");
  ImGui::Text("%d trees; %d can grow",
              int(component.trees.size()),
              num_growable_trees(component.trees));
  ImGui::Checkbox("ShowTreeStats", &show_tree_stats);

#if 1
  if (show_tree_stats) {
    auto growth_inst = tree::read_growth_context(growth_system, component.growth_context);
    size_t num_points{};
    size_t num_available_points{};
    size_t num_oct_nodes{};
    if (growth_inst.attraction_points) {
      num_points = growth_inst.attraction_points->count_non_empty();
      num_available_points = tree::count_num_available_attraction_points(
        *growth_inst.attraction_points);
      num_oct_nodes = growth_inst.attraction_points->num_nodes();
    }

    float last_growth_time_ms{};
    if (growth_inst.growth_result) {
      last_growth_time_ms = float(growth_inst.growth_result->elapsed_time * 1e3);
    }

    ImGui::Text("Last growth: %0.2f ms", float(last_growth_time_ms));
    ImGui::Text("Attraction points: %d", int(num_points));
    ImGui::Text("Avail points: %d", int(num_available_points));
    ImGui::Text("Num oct nodes: %d", int(num_oct_nodes));
#if 0
    push_scratch_diameters(component.trees);
    auto mean_diameter = mean_scratch();
    auto std_diameter = std_scratch();
    ImGui::Text("%0.2f mean; %0.2f std diameter.", mean_diameter, std_diameter);

    push_num_internodes(component.trees);
    auto mean_num = mean_scratch();
    auto std_num = std_scratch();
    ImGui::Text("%0.2f mean; %0.2f std num internodes.", mean_num, std_num);

    push_num_leaves(component.trees);
    auto mean_num_leaves = mean_scratch();
    auto std_num_leaves = std_scratch();
    ImGui::Text("%0.2f mean; %0.2f std num leaves.", mean_num_leaves, std_num_leaves);
#endif
    ImGui::Text("Pollen particles: %d", int(component.active_pollen_particles.size()));
  }
#endif

  if (ImGui::Button("ResetTformPosition")) {
    component.need_reset_tform_position = true;
  }

  if (ImGui::Button("NewTree")) {
    result.make_new_tree = true;
  }

  if (ImGui::Button("AddTreeAtTformPosition")) {
    result.add_tree_at_tform_position = true;
  }

  if (ImGui::Button("AddTreesAtOrigin")) {
    result.make_trees_at_origin = true;
  }

//  if (ImGui::Button("RemakeDrawables")) {
//    result.remake_drawables = true;
//  }

  if (ImGui::InputInt("AttractionPointsType", &attraction_points_type)) {
    result.attraction_points_type = attraction_points_type;
  }

  int spawn_p_type = component.spawn_params_type;
  if (ImGui::InputInt("SpawnParamsType", &spawn_p_type)) {
    result.spawn_params_type = spawn_p_type;
  }

  bool is_pine = component.is_pine;
  if (ImGui::Checkbox("IsPine", &is_pine)) {
    result.is_pine = is_pine;
  }

  int foliage_leaves_type = component.foliage_leaves_type;
  if (ImGui::InputInt("FoliageLeavesType", &foliage_leaves_type)) {
    result.foliage_leaves_type = foliage_leaves_type;
  }

  auto tree_spawn_enabled = component.tree_spawn_enabled;
  if (ImGui::Checkbox("TreeSpawnEnabled", &tree_spawn_enabled)) {
    result.tree_spawn_enabled = tree_spawn_enabled;
  }

  bool add_flower_patch_after_growing = component.add_flower_patch_after_growing;
  if (ImGui::Checkbox("AddFlowerPatchAfterGrowing", &add_flower_patch_after_growing)) {
    result.add_flower_patch_after_growing = add_flower_patch_after_growing;
  }

  bool grow_vines_by_signal = component.grow_vines_by_signal;
  if (ImGui::Checkbox("GrowVinesBySignal", &grow_vines_by_signal)) {
    result.vine_growth_by_signal = grow_vines_by_signal;
  }

  bool hide_foliage_drawable_components = component.hide_foliage_drawable_components;
  if (ImGui::Checkbox("HideFoliageDrawComponents", &hide_foliage_drawable_components)) {
    result.hide_foliage_drawable_components = hide_foliage_drawable_components;
  }

  bool render_attrac_points = tree::debug::is_debug_growth_context_point_drawable_active(
    component.growth_context);
  if (ImGui::Checkbox("RenderAttractionPoints", &render_attrac_points)) {
    result.render_attraction_points = render_attrac_points;
  }

  bool render_node_skeleton = component.render_node_skeleton;
  if (ImGui::Checkbox("RenderNodeSkeleton", &render_node_skeleton)) {
    result.render_node_skeleton = render_node_skeleton;
  }

  bool wind_influence_enabled = component.wind_influence_enabled;
  if (ImGui::Checkbox("WindInfluenceEnabled", &wind_influence_enabled)) {
    result.wind_influence_enabled = wind_influence_enabled;
  }

  bool axis_growth_by_signal = component.axis_growth_by_signal;
  if (ImGui::Checkbox("AxisGrowthBySignal", &axis_growth_by_signal)) {
    result.axis_growth_by_signal = axis_growth_by_signal;
  }

  bool can_trigger_death = component.can_trigger_death;
  if (ImGui::Checkbox("CanTriggerDeath", &can_trigger_death)) {
    result.can_trigger_death = can_trigger_death;
  }

  float growth_incr = component.axis_growth_incr;
  if (ImGui::SliderFloat("AxisGrowthIncr", &growth_incr, 0.001f, 0.5f)) {
    result.axis_growth_incr = growth_incr;
  }

  bool disable_static_leaves = component.disable_static_leaves;
  if (ImGui::Checkbox("DisableStaticLeaves", &disable_static_leaves)) {
    result.disable_static_leaves = disable_static_leaves;
  }

  bool disable_foliage_components = component.disable_foliage_components;
  if (ImGui::Checkbox("DisableFoliageComponents", &disable_foliage_components)) {
    result.disable_foliage_components = disable_foliage_components;
  }

//  bool use_static_leaves = component.use_static_leaves;
//  if (ImGui::Checkbox("UseStaticLeaves", &use_static_leaves)) {
//    result.use_static_leaves = use_static_leaves;
//  }
  bool use_hemisphere_color_image = component.use_hemisphere_color_image;
  if (ImGui::Checkbox("UseHemisphereColorImage", &use_hemisphere_color_image)) {
    result.use_hemisphere_color_image = use_hemisphere_color_image;
  }
  bool randomize_hemisphere_color_images = component.randomize_hemisphere_color_images;
  if (ImGui::Checkbox("RandomizeHemisphereColorImages", &randomize_hemisphere_color_images)) {
    result.randomize_hemisphere_color_images = randomize_hemisphere_color_images;
  }
//  bool randomize_static_or_proc_leaves = component.randomize_static_or_proc_leaves;
//  if (ImGui::Checkbox("RandomizeStaticOrProcLeaves", &randomize_static_or_proc_leaves)) {
//    result.randomize_static_or_proc_leaves = randomize_static_or_proc_leaves;
//  }
//  bool small_proc_leaves = component.always_small_proc_leaves;
//  if (ImGui::Checkbox("AlwaysSmallProcLeaves", &small_proc_leaves)) {
//    result.always_small_proc_leaves = small_proc_leaves;
//  }

  float proc_wind_fast_osc_scale = component.proc_wind_fast_osc_amplitude_scale;
  if (ImGui::SliderFloat("ProcWindFastOscScale", &proc_wind_fast_osc_scale, 0.0f, 10.0f)) {
    result.proc_wind_fast_osc_scale = proc_wind_fast_osc_scale;
  }

  float static_wind_fast_osc_scale = component.static_wind_fast_osc_amplitude_scale;
  if (ImGui::SliderFloat("StaticWindFastOscScale", &static_wind_fast_osc_scale, 0.0f, 1.0f)) {
    result.static_wind_fast_osc_scale = static_wind_fast_osc_scale;
  }

  float signal_axis_growth_scale = component.signal_axis_growth_incr_scale;
  if (ImGui::SliderFloat("SignalAxisGrowthScale", &signal_axis_growth_scale, 0.0f, 1.0f)) {
    result.signal_axis_growth_scale = signal_axis_growth_scale;
  }

  float signal_leaf_growth_scale = component.signal_leaf_growth_incr_scale;
  if (ImGui::SliderFloat("SignalLeafGrowthScale", &signal_leaf_growth_scale, 0.0f, 1.0f)) {
    result.signal_leaf_growth_scale = signal_leaf_growth_scale;
  }

  float spiral_theta = component.resource_spiral_global_particle_theta;
  if (ImGui::SliderFloat("ResourceSpiralTheta", &spiral_theta, -pif(), pif())) {
    result.resource_spiral_theta = spiral_theta;
  }
  float spiral_vel = component.resource_spiral_global_particle_velocity;
  if (ImGui::SliderFloat("ResourceSpiralVel", &spiral_vel, 0.0f, 24.0f)) {
    result.resource_spiral_vel = spiral_vel;
  }

  int num_add = component.num_trees_manually_add;
  if (ImGui::InputInt("NumTreesAdd", &num_add)) {
    result.num_trees_manually_add = num_add;
  }

  Vec3f tree_ori = component.default_new_tree_origin;
  if (ImGui::InputFloat3("TreeOrigin", &tree_ori.x, "%0.3f")) {
    result.tree_origin = tree_ori;
  }

  float tree_ori_span = component.new_tree_origin_span;
  if (ImGui::InputFloat("TreeOriginSpan", &tree_ori_span)) {
    result.tree_origin_span = tree_ori_span;
  }

  if (ImGui::InputInt("PruneSelectedAxisIndex", &prune_selected_axis_index)) {
    prune_selected_axis_index = std::max(0, prune_selected_axis_index);
  }
  if (ImGui::Button("PruneAxis")) {
    result.prune_selected_axis_index = prune_selected_axis_index;
  }

  if (ImGui::Button("Close")) {
    result.close = true;
  }

  ImGui::Checkbox("ShowSelectableTrees", &show_selectable_trees);

  if (show_selectable_trees) {
    int tree_ind{};
    for (auto& [id, tree] : component.trees) {
#if 0
      const auto num_nodes = int(tree.nodes.internodes.size());
      const auto num_buds = int(tree.nodes.buds.size());
      const auto max_grav_order = max_gravelius_order(tree.nodes.internodes);
#else
      //  @TODO
      const auto num_nodes = 0;
      const auto num_buds = 0;
      const auto max_grav_order = 0;
#endif

      std::string select_str{"Select"};
      select_str += std::to_string(tree_ind);
      if (ImGui::SmallButton(select_str.c_str())) {
        result.selected_tree = id;
      }

      ImGui::SameLine();
      ImGui::Text("%d: %d nodes, %d buds, %d max order",
                  tree_ind++, num_nodes, num_buds, max_grav_order);
    }
  }

  {
    char buff[1024];
    memset(buff, 0, 1024);
    if (ImGui::InputText("SerializeToFile", buff, 1024, enter_flag())) {
      result.serialize_selected_to_file_path = std::string{buff};
    }

    memset(buff, 0, 1024);
    if (ImGui::InputText("DeserializeFromFile", buff, 1024, enter_flag())) {
      result.deserialize_from_file_path = std::string{buff};
    }

    auto deser_trans = component.deserialized_tree_translation;
    if (ImGui::InputFloat3("DeserializedTreeTranslation", &deser_trans.x, "%0.3f", enter_flag())) {
      result.deserialized_tree_translation = deser_trans;
    }
  }


  ImGui::End();
  return result;
}

GROVE_NAMESPACE_END