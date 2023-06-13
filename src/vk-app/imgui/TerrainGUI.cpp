#include "TerrainGUI.hpp"
#include "../terrain/DebugTerrainComponent.hpp"
#include "grove/common/common.hpp"
#include "grove/math/constants.hpp"
#include <imgui/imgui.h>

GROVE_NAMESPACE_BEGIN

TerrainGUIUpdateResult TerrainGUI::render(const DebugTerrainComponent& component) {
  TerrainGUIUpdateResult result{};
  ImGui::Begin("TerrainGUI");

  constexpr auto enter_flag = ImGuiInputTextFlags_EnterReturnsTrue;

  if (ImGui::Button("UpdateAltTerrainColorImage")) {
    result.alt_terrain_color_image_file_path = "/textures/grass/new_terrain_experiment.png";
  }

  if (ImGui::TreeNode("NodeIntersect")) {
    if (auto rot = component.get_roots_rotation()) {
      auto rotv = rot.value();
      if (ImGui::SliderFloat2("Rotation", &rotv.x, 0.0f, float(two_pi()))) {
        result.debug_roots_rotation = rotv;
      }
    }

    bool keep_axis = component.nodes_through_terrain_params.keep_axis;
    if (ImGui::Checkbox("KeepAxis", &keep_axis)) {
      result.keep_axis = keep_axis;
    }

    int keep_ith_axis = component.nodes_through_terrain_params.keep_ith_axis;
    if (ImGui::InputInt("KeepIthAxis", &keep_ith_axis)) {
      result.keep_ith_axis = keep_ith_axis;
    }

    ImGui::TreePop();
  }

  if (ImGui::TreeNode("PlaceOnMesh")) {
    if (ImGui::Button("Recompute")) {
      result.recompute_mesh_projected_bounds = true;
    }
    auto sz = component.place_on_mesh_params.obb3_size;
    if (ImGui::InputFloat3("OBB3Size", &sz.x)) {
      result.mesh_obb3_size = sz;
    }

    float normal_y_thresh = component.place_on_mesh_params.normal_y_threshold;
    if (ImGui::SliderFloat("NormalYThreshold", &normal_y_thresh, 0.0f, 1.0f)) {
      result.place_on_mesh_normal_y_threshold = normal_y_thresh;
    }

    bool draw_result = component.place_on_mesh_params.draw_result;
    if (ImGui::Checkbox("DrawResult", &draw_result)) {
      result.draw_place_on_mesh_result = draw_result;
    }

    ImGui::TreePop();
  }

  if (ImGui::TreeNode("CubeMarch")) {
    auto stats = component.get_cube_march_stats();
    ImGui::Text("NumVoxelSamples: %d", stats.num_voxel_samples);
    ImGui::Text("NumVoxelBlocks: %d", stats.num_voxel_blocks);
    ImGui::Text("NumCubeMarchTriangles: %d", stats.num_cube_march_triangles);
    ImGui::Text("NumCubeMarchVertices: %d", stats.num_cube_march_vertices);
    ImGui::Text("NumCubeMarchChunks: %d", stats.num_cube_march_chunks);

    if (ImGui::Button("RecomputeCubeMarchGeometry")) {
      result.recompute_cube_march_geometry = true;
    }
    if (ImGui::Button("ClearCubeMarchGeometry")) {
      result.clear_cube_march_geometry = true;
    }

    float radius = component.get_cube_march_editor_radius();
    if (ImGui::SliderFloat("Radius", &radius, 0.0f, 32.0f)) {
      result.cube_march_editor_radius = radius;
    }

    bool inv = component.cube_march_params.invert;
    if (ImGui::Checkbox("Invert", &inv)) {
      result.invert_cube_march_tool = inv;
    }

    bool active = component.cube_march_params.active;
    if (ImGui::Checkbox("Active", &active)) {
      result.cube_march_editing_active = active;
    }

    bool hidden = component.cube_march_params.hidden;
    if (ImGui::Checkbox("Hidden", &hidden)) {
      result.cube_march_hidden = hidden;
    }

    bool use_brush = component.cube_march_params.use_wall_brush;
    if (ImGui::Checkbox("UseWallBrush", &use_brush)) {
      result.cube_march_use_wall_brush = use_brush;
    }

    bool instr_control = component.cube_march_params.brush_control_by_instrument;
    if (ImGui::Checkbox("WallBrushControlByInstrument", &instr_control)) {
      result.cube_march_control_wall_brush_by_instrument = instr_control;
    }

    bool draw_bounds = component.cube_march_params.draw_bounds;
    if (ImGui::Checkbox("DrawBounds", &draw_bounds)) {
      result.cube_march_draw_bounds = draw_bounds;
    }

    float speed = component.cube_march_params.wall_brush_speed;
    if (ImGui::SliderFloat("WallBrushSpeed", &speed, 0.0f, 8.0f)) {
      result.cube_march_wall_brush_speed = speed;
    }

    float rand_weight = component.cube_march_params.wall_random_axis_weight;
    if (ImGui::SliderFloat("WallBrushRandomAxisWeight", &rand_weight, 0.0f, 2.0f)) {
      result.cube_march_wall_random_axis_weight = rand_weight;
    }

    float circ_scale = component.cube_march_params.wall_brush_circle_scale;
    if (ImGui::SliderFloat("WallBrushCircleScale", &circ_scale, -0.05f, 0.05f)) {
      result.cube_march_wall_circle_scale = circ_scale;
    }

    bool allow_recede = component.cube_march_params.allow_wall_recede;
    if (ImGui::Checkbox("AllowWallRecede", &allow_recede)) {
      result.allow_cube_march_wall_recede = allow_recede;
    }

    if (ImGui::Button("IncreaseWallHeight")) {
      result.need_increase_cube_march_wall_height = true;
    }
    if (ImGui::Button("DecreaseWallHeight")) {
      result.need_decrease_cube_march_wall_height = true;
    }
    if (ImGui::Button("ReinitializeWall")) {
      result.need_reinitialize_cube_march_wall = true;
    }

    {
      char text[1024];
      memset(text, 0, 1024);
      if (ImGui::InputText("SplotchImageFilePath", text, 1024, enter_flag)) {
        std::string path{text};
        result.splotch_image_file_path = std::move(path);
      }

      memset(text, 0, 1024);
      if (ImGui::InputText("GroundColorImageFilePath", text, 1024, enter_flag)) {
        std::string path{text};
        result.alt_terrain_color_image_file_path = std::move(path);
      }
    }

    ImGui::TreePop();
  }

  if (component.debug_model_index < int(component.debug_tforms.size())) {
    auto& trs = component.debug_tforms[component.debug_model_index]->get_current();
    auto scl = trs.scale;
    if (ImGui::InputFloat3("Scale", &scl.x)) {
      result.model_scale = scl;
    }
    auto trans = trs.translation;
    if (ImGui::InputFloat3("Translation", &trans.x)) {
      result.model_translation = trans;
    }
  }

  {
    char text[1024];
    memset(text, 0, 1024);
    if (ImGui::InputText("GeometryFilePath", text, 1024, enter_flag)) {
      std::string path{text};
      result.geometry_file_path = std::move(path);
    }

    memset(text, 0, 1024);
    if (ImGui::InputText("ImageFilePath", text, 1024, enter_flag)) {
      std::string path{text};
      result.image_file_path = std::move(path);
    }
  }

  if (ImGui::Button("LoadRock")) {
    result.geometry_file_path = "rock/geom1.obj";
    result.image_file_path = "rock/geom1_im.png";
    result.model_scale = Vec3f{4.0f, 8.0f, 4.0f};
    result.model_translation = Vec3f{32.0f, 8.0f, 0.0f};
  }

  int mi = component.debug_model_index;
  if (ImGui::InputInt("ModelIndex", &mi) && mi >= 0 && mi < int(component.debug_models.size())) {
    result.model_index = mi;
  }

  if (ImGui::Button("AddModel")) {
    result.add_model = true;
  }

  if (ImGui::Button("Close")) {
    result.close = true;
  }

  ImGui::End();
  return result;
}

GROVE_NAMESPACE_END
