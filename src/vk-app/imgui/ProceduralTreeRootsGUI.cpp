#include "ProceduralTreeRootsGUI.hpp"
#include "../procedural_tree/DebugTreeRootsComponent.hpp"
#include "grove/common/common.hpp"
#include <imgui/imgui.h>

GROVE_NAMESPACE_BEGIN

ProceduralTreeRootsGUIUpdateResult
ProceduralTreeRootsGUI::render(const bounds::RadiusLimiter* roots_radius_limiter,
                               const DebugTreeRootsComponent& debug_component) {
  ProceduralTreeRootsGUIUpdateResult result{};
  ImGui::Begin("ProceduralTreeRootsGUI");

  if (ImGui::TreeNode("RadiusLimiter")) {
    bool validate = debug_component.params.validate_radius_limiter;
    auto radius_lim_stats = bounds::get_stats(roots_radius_limiter);
    ImGui::Text("NumElements: %d", radius_lim_stats.num_elements);
    ImGui::Text("NumFreeElements: %d", radius_lim_stats.num_free_elements);
    ImGui::Text("NumCells: %d", radius_lim_stats.num_cells);
    ImGui::Text("NumCellIndices: %d", radius_lim_stats.num_cell_indices);
    ImGui::Text("NumFreeCellIndices: %d", radius_lim_stats.num_free_cell_indices);
    ImGui::Text("NumElementIndices: %d", radius_lim_stats.num_element_indices);
    ImGui::Text("NumFreeElementIndices: %d", radius_lim_stats.num_free_element_indices);

    const auto any_radius_constrained = int(
      debug_component.any_root_nodes_radius_constrained(roots_radius_limiter));
    ImGui::Text("AnyRootRadiusConstrained: %d", any_radius_constrained);

    if (ImGui::Checkbox("ValidateRadiusLimiter", &validate)) {
      result.validate_radius_limiter = validate;
    }

    bool draw_cube_grid = debug_component.params.draw_cube_grid;
    if (ImGui::Checkbox("DrawCubeGrid", &draw_cube_grid)) {
      result.draw_cube_grid = draw_cube_grid;
    }
    bool debug_draw_enabled = debug_component.params.debug_draw_enabled;
    if (ImGui::Checkbox("DebugDrawEnabled", &debug_draw_enabled)) {
      result.debug_draw_enabled = debug_draw_enabled;
    }
    ImGui::TreePop();
  }

  ImGui::Text("NumRootAggregates: %d", debug_component.num_root_aggregates());
  ImGui::Text("MaxRadius: %0.2f", debug_component.max_radius());
  ImGui::Text("NumGrowing: %d", debug_component.num_growing());
  ImGui::Text("NumReceding: %d", debug_component.num_receding());

  if (ImGui::TreeNode("GrowthOnNodes")) {
    if (ImGui::Button("GenerateSamplePoints")) {
      result.generate_sample_points = true;
    }
    float rad_off = debug_component.params.points_on_nodes_radius_offset;
    if (ImGui::SliderFloat("RadiusOffset", &rad_off, 0.0f, 1.0f)) {
      result.points_on_nodes_radius_offset = rad_off;
    }
    float step_size = debug_component.params.points_on_nodes_step_size;
    if (ImGui::SliderFloat("StepSize", &step_size, 0.1f, 2.0f)) {
      result.points_on_nodes_step_size = step_size;
    }

    float diam = debug_component.params.points_on_nodes_leaf_diameter;
    if (ImGui::SliderFloat("LeafDiameter", &diam, 0.01f, 0.5f)) {
      result.points_on_nodes_leaf_diameter = diam;
    }
    float diam_pow = debug_component.params.points_on_nodes_diameter_power;
    if (ImGui::SliderFloat("DiameterPower", &diam_pow, 1.0f, 3.0f)) {
      result.points_on_nodes_diameter_power = diam_pow;
    }

    auto& col = debug_component.params.points_on_nodes_color;
    int color_xyz[3]{col[0], col[1], col[2]};
    if (ImGui::SliderInt3("Color", color_xyz, 0, 255)) {
      result.points_on_nodes_color = Vec3<uint8_t>{
        uint8_t(color_xyz[0]), uint8_t(color_xyz[1]), uint8_t(color_xyz[2])};
    }

    bool smooth_points = debug_component.params.smooth_points_on_nodes;
    if (ImGui::Checkbox("Smooth", &smooth_points)) {
      result.smooth_points_on_nodes = smooth_points;
    }

    bool target_down = debug_component.params.points_on_nodes_step_axis.y == -1.0f;
    if (ImGui::Checkbox("TargetDown", &target_down)) {
      result.points_on_nodes_target_down = target_down;
    }

    bool prefer_entry_up = debug_component.params.points_on_nodes_prefer_entry_up_axis;
    if (ImGui::Checkbox("PreferEntryUpAxis", &prefer_entry_up)) {
      result.points_on_nodes_prefer_entry_up_axis = prefer_entry_up;
    }

    auto cast_v3i = [](const Vec3<uint8_t>& v) {
      return Vec3<int>{int(v.x), int(v.y), int(v.z)};
    };

    auto cast_u8 = [](const Vec3<int>& v) {
      return Vec3<uint8_t>{uint8_t(v.x), uint8_t(v.y), uint8_t(v.z)};
    };

    Vec3<int> colors[4] = {
      cast_v3i(debug_component.params.material1_colors.c0),
      cast_v3i(debug_component.params.material1_colors.c1),
      cast_v3i(debug_component.params.material1_colors.c2),
      cast_v3i(debug_component.params.material1_colors.c3),
    };
    bool has_colors{};
    for (int i = 0; i < 4; i++) {
      std::string color_label{"Color"};
      color_label += std::to_string(i);
      if (ImGui::SliderInt3(color_label.c_str(), &colors[i].x, 0, 255)) {
        has_colors = true;
      }
    }

    if (has_colors) {
      ProceduralTreeRootsGUIUpdateResult::Material1Colors mat_colors{};
      mat_colors.c0 = cast_u8(colors[0]);
      mat_colors.c1 = cast_u8(colors[1]);
      mat_colors.c2 = cast_u8(colors[2]);
      mat_colors.c3 = cast_u8(colors[3]);
      result.material1_colors = mat_colors;
    }

    if (ImGui::Button("SetPreset1")) {
      result.set_points_on_nodes_preset1 = true;
    }

    ImGui::TreePop();
  }

  bool make_tree = debug_component.params.make_tree;
  if (ImGui::Checkbox("MakeTree", &make_tree)) {
    result.make_tree = make_tree;
  }

  bool scale_growth_rate_by_signal = debug_component.params.scale_growth_rate_by_signal;
  if (ImGui::Checkbox("ScaleGrowthRateBySignal", &scale_growth_rate_by_signal)) {
    result.scale_growth_rate_by_signal = scale_growth_rate_by_signal;
  }

#if 0
  float diam_scale = debug_component.params.diameter_scale;
  if (ImGui::SliderFloat("DiameterScale", &diam_scale, 0.5f, 4.0f)) {
    result.diameter_scale = diam_scale;
  }
#endif

  float attrac_scale = debug_component.params.attractor_point_scale;
  if (ImGui::SliderFloat("AttractorPointScale", &attrac_scale, -0.1f, 0.1f)) {
    result.attractor_point_scale = attrac_scale;
  }

  float growth_rate = debug_component.params.growth_rate;
  if (ImGui::SliderFloat("GrowthRate", &growth_rate, 0.0f, 4.0f)) {
    result.growth_rate = growth_rate;
  }

  bool pref_glob = debug_component.params.prefer_global_p_spawn_lateral;
  if (ImGui::Checkbox("PreferGlobalPSpawnLateral", &pref_glob)) {
    result.prefer_global_p_spawn_lateral = pref_glob;
  }

  int selected_root_index = debug_component.params.selected_root_index;
  if (ImGui::InputInt("SelectedRootIndex", &selected_root_index) && selected_root_index >= 0) {
    result.selected_root_index = selected_root_index;
  }

  int selected_node_index = debug_component.params.selected_node_index;
  if (ImGui::InputInt("SelectedNodeIndex", &selected_node_index) && selected_node_index >= 0) {
    result.selected_node_index = selected_node_index;
  }

  bool add_at_ori = debug_component.params.add_roots_at_new_tree_origins;
  if (ImGui::Checkbox("AddAtNewTreeOrigins", &add_at_ori)) {
    result.add_roots_at_new_tree_origins = add_at_ori;
  }

  bool allow_recede = debug_component.params.allow_recede;
  if (ImGui::Checkbox("AllowRecede", &allow_recede)) {
    result.allow_recede = allow_recede;
  }

  bool cam_pos_attractor = debug_component.params.camera_position_attractor;
  if (ImGui::Checkbox("CameraPositionAttractor", &cam_pos_attractor)) {
    result.camera_position_attractor = cam_pos_attractor;
  }

  bool draw_node_frames = debug_component.params.draw_node_frames;
  if (ImGui::Checkbox("DrawNodeFrames", &draw_node_frames)) {
    result.draw_node_frames = draw_node_frames;
  }

  auto p_spawn_lateral = float(debug_component.params.p_spawn_lateral);
  if (ImGui::SliderFloat("PSpawnLateral", &p_spawn_lateral, 0.0f, 0.5f)) {
    result.p_spawn_lateral = p_spawn_lateral;
  }

  float min_len = debug_component.params.min_axis_length_spawn_lateral;
  if (ImGui::SliderFloat("MinAxisLengthSpawnLateral", &min_len, 0.0f, 32.0f)) {
    result.min_axis_length_spawn_lateral = min_len;
  }

  float leaf_diam = debug_component.params.leaf_diameter;
  if (ImGui::SliderFloat("LeafDiameter", &leaf_diam, 0.025f, 0.25f)) {
    result.leaf_diameter = leaf_diam;
  }

  float diam_power = debug_component.params.diameter_power;
  if (ImGui::SliderFloat("DiameterPower", &diam_power, 1.0f, 3.0f)) {
    result.diameter_power = diam_power;
  }

  float node_len = debug_component.params.node_length;
  if (ImGui::SliderFloat("NodeLength", &node_len, 0.25f, 2.0f)) {
    result.node_length = node_len;
  }

  bool wind_disabled = debug_component.params.wind_disabled;
  if (ImGui::Checkbox("WindDisabled", &wind_disabled)) {
    result.wind_disabled = wind_disabled;
  }

  if (ImGui::Button("SpawnAxis")) {
    result.spawn_axis = true;
  }

  if (ImGui::Button("FitBoundsAroundAxis")) {
    result.need_fit_bounds_around_axis = true;
  }

  bool add_at_tform = debug_component.params.add_roots_at_tform;
  if (ImGui::Checkbox("AddRootsAtTransform", &add_at_tform)) {
    result.add_roots_at_transform = add_at_tform;
  }

  Vec3f root_ori = debug_component.params.default_root_origin;
  if (ImGui::InputFloat3("DefaultRootOrigin", &root_ori.x)) {
    result.default_root_origin = root_ori;
  }

  float root_ori_span = debug_component.params.rand_root_origin_span;
  if (ImGui::InputFloat("RootOriginSpan", &root_ori_span)) {
    result.rand_root_origin_span = root_ori_span;
  }

  int max_num_nodes_per_roots = debug_component.params.max_num_nodes_per_roots;
  if (ImGui::InputInt("MaxNumNodesPerRoots", &max_num_nodes_per_roots)) {
    result.max_num_nodes_per_roots = max_num_nodes_per_roots;
  }

  if (ImGui::Button("CreateRoots")) {
    result.create_roots = true;
  }

  if (ImGui::Button("CreateShortTree")) {
    result.create_short_tree = true;
  }

  int num_create = debug_component.params.num_roots_create;
  if (ImGui::InputInt("NumRootsCreate", &num_create)) {
    result.num_roots_create = num_create;
  }

  {
    char text[1024];
    memset(text, 0, 1024);
    if (ImGui::InputText("Deserialize", text, 1024, ImGuiInputTextFlags_EnterReturnsTrue)) {
      std::string path{text};
      result.deserialize = std::move(path);
    }

    memset(text, 0, 1024);
    if (ImGui::InputText("Serialize", text, 1024, ImGuiInputTextFlags_EnterReturnsTrue)) {
      std::string path{text};
      result.serialize = std::move(path);
    }
  }

  if (ImGui::Button("Close")) {
    result.close = true;
  }

  ImGui::End();
  return result;
}

GROVE_NAMESPACE_END
