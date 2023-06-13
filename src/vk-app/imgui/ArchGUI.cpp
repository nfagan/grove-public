#include "ArchGUI.hpp"
#include "../architecture/DebugArchComponent.hpp"
#include "grove/common/common.hpp"
#include "grove/math/constants.hpp"
#include <imgui/imgui.h>
#include <string>

GROVE_NAMESPACE_BEGIN

namespace {

constexpr auto enter_flag() {
  return ImGuiInputTextFlags_EnterReturnsTrue;
}

std::string make_tagged_label(const char* p, int id) {
  std::string result{p};
  result += "##";
  result += std::to_string(id);
  return result;
}

Optional<arch::WallHole> render_wall_hole(arch::WallHole hole) {
  Optional<arch::WallHole> result;
  if (ImGui::SliderFloat("Rot", &hole.rot, 0.0f, 2.0f * pif(), "%0.3f")) {
    result = hole;
  }
  if (ImGui::SliderFloat2("Scale", &hole.scale.x, 0.01f, 0.5f, "%0.3f")) {
    result = hole;
  }
  if (ImGui::SliderFloat2("Off", &hole.off.x, -0.5f, 0.5f, "%0.3f")) {
    result = hole;
  }
  if (ImGui::SliderFloat("Curl", &hole.curl, 0.15f, 0.8f, "%0.3f")) {
    result = hole;
  }
  return result;
}

bool default_input_float(const char* label, float* v) {
  return ImGui::InputFloat(label, v, 0.0f, 0.0f, "%0.3f", enter_flag());
}

bool default_input_float2(const char* label, float* v) {
  return ImGui::InputFloat2(label, v, "%0.3f", enter_flag());
}

bool default_input_float3(const char* label, float* v) {
  return ImGui::InputFloat3(label, v, "%0.3f", enter_flag());
}

bool default_slider_float3(const char* label, float* v, float mn, float mx) {
  return ImGui::SliderFloat3(label, v, mn, mx);
}

bool default_slider_float(const char* label, float* v, float mn, float mx) {
  return ImGui::SliderFloat(label, v, mn, mx);
}

#define SET_RESULT_IF(cond) \
  if ((cond)) {             \
    has_result = true;      \
  }

Optional<ArchGUIUpdateResult::CollideThroughHoleParams>
render_collide_through_hole_params(const DebugArchComponent& component,
                                   ArchGUIUpdateResult& gui_res) {
  using P = ArchGUIUpdateResult::CollideThroughHoleParams;
  auto& p = component.collide_through_hole_params;
  P result;
  result.collider_scale = component.obb_isect_collider_tform->get_current().scale;
  result.wall_scale = component.obb_isect_wall_tform->get_current().scale;
  result.collider_angles = p.collider_angles;
  result.wall_angles = p.wall_angles;
  result.forward_dim = p.forward_dim;
  result.with_tree_nodes = p.with_tree_nodes;
  result.min_collide_node_diam = p.min_collide_node_diam;
  result.projected_aabb_scale = p.projected_aabb_scale;
  result.hole_curl = p.hole_curl;
  result.continuous_compute = p.continuous_compute;
  result.prune_initially_rejected = p.prune_initially_rejected;
  result.leaf_obb_scale = p.leaf_obb_scale;
  result.leaf_obb_offset = p.leaf_obb_offset;
  result.reject_all_holes = p.reject_all_holes;
  bool has_result = false;
  SET_RESULT_IF(default_input_float3("ColliderScale", &result.collider_scale.x))
  SET_RESULT_IF(default_input_float3("WallScale", &result.wall_scale.x))
  SET_RESULT_IF(default_slider_float3("ColliderAngles", &result.collider_angles.x, 0.0f, pif()))
  SET_RESULT_IF(default_slider_float3("WallAngles", &result.wall_angles.x, 0.0f, pif()))
  SET_RESULT_IF(ImGui::InputInt("ForwardDim", &result.forward_dim))
  SET_RESULT_IF(ImGui::Checkbox("WithTreeNodes", &result.with_tree_nodes))
  SET_RESULT_IF(ImGui::Checkbox("ContinuousCompute", &result.continuous_compute))
  SET_RESULT_IF(ImGui::Checkbox("PruneInitiallyRejected", &result.prune_initially_rejected))
  SET_RESULT_IF(ImGui::Checkbox("RejectAllHoles", &result.reject_all_holes))
  SET_RESULT_IF(ImGui::SliderFloat("MinNodeDiam", &result.min_collide_node_diam, 0.0f, 1.0f))
  SET_RESULT_IF(ImGui::SliderFloat("ProjectedAABBScale", &result.projected_aabb_scale, 0.0f, 4.0f))
  SET_RESULT_IF(ImGui::SliderFloat("HoleCurl", &result.hole_curl, 0.1f, 0.8f))
  SET_RESULT_IF(default_input_float3("LeafOBBScale", &result.leaf_obb_scale.x))
  SET_RESULT_IF(default_input_float3("LeafOBBOffset", &result.leaf_obb_offset.x))
  if (ImGui::Button("RecomputeGeometry")) {
    gui_res.recompute_collide_through_hole_geometry = true;
  }
  return has_result ? Optional<P>(result) : NullOpt{};
}

Optional<ArchGUIUpdateResult::RenderGrowthParams>
render_render_growth_params(const DebugArchComponent& component) {
  using P = ArchGUIUpdateResult::RenderGrowthParams;
  auto& p = component.render_growth_params;
  P result;
  result.retrigger_growth = false;
  result.retrigger_recede = false;
  result.growth_incr = p.growth_incr;
  result.grow_by_instrument = p.grow_by_instrument;
  result.instrument_scale = p.instrument_scale;
  bool has_result = false;
  SET_RESULT_IF(default_slider_float("GrowthIncr", &result.growth_incr, 0.0f, 1.0f))
  SET_RESULT_IF(default_slider_float("InstrumentScale", &result.instrument_scale, 0.0f, 1.0f))
  SET_RESULT_IF(ImGui::Checkbox("GrowByInstrument", &result.grow_by_instrument))
  if (ImGui::Button("RetriggerGrowth")) {
    result.retrigger_growth = true;
    has_result = true;
  }
  if (ImGui::Button("RetriggerRecede")) {
    result.retrigger_recede = true;
    has_result = true;
  }
  return has_result ? Optional<P>(result) : NullOpt{};
}

Optional<ArchGUIUpdateResult::StructureGrowthParams>
render_structure_growth_params(const DebugArchComponent& component, ArchGUIUpdateResult& gui_res) {
  using P = ArchGUIUpdateResult::StructureGrowthParams;
  auto& p = component.structure_growth_params;
  P result;
  result.dist_begin_propel = p.encircle_point_params.dist_begin_propel;
  result.dist_attract_until = p.encircle_point_params.dist_attract_until;
  result.attract_force_scale = p.encircle_point_params.attract_force_scale;
  result.propel_force_scale = p.encircle_point_params.propel_force_scale;
  result.dt = p.encircle_point_params.dt;
  result.num_pieces = p.num_pieces;
  result.piece_length = p.piece_length;
  result.structure_ori = p.structure_ori;
  result.use_variable_piece_length = p.use_variable_piece_length;
  result.set_preset1 = false;
  result.target_length = p.target_length;
  result.use_isect_wall_obb = p.use_isect_wall_obb;
  result.auto_extrude = p.auto_extrude;
  result.randomize_wall_scale = p.randomize_wall_scale;
  result.randomize_piece_type = p.randomize_piece_type;
  result.restrict_structure_x_length = p.restrict_structure_x_length;
  result.auto_project_internodes = p.auto_project_internodes;
  result.delay_to_recede_s = p.delay_to_recede_s;
  result.allow_recede = p.allow_recede;
  bool has_result = false;
  SET_RESULT_IF(default_input_float("DistBeginPropel", &result.dist_begin_propel))
  SET_RESULT_IF(default_input_float("DistAttractUntil", &result.dist_attract_until))
  SET_RESULT_IF(default_input_float("AttractForceScale", &result.attract_force_scale))
  SET_RESULT_IF(default_input_float("PropelForceScale", &result.propel_force_scale))
  SET_RESULT_IF(default_input_float("PieceLength", &result.piece_length))
  SET_RESULT_IF(default_input_float3("StructureOrigin", &result.structure_ori.x))
  SET_RESULT_IF(ImGui::InputInt("NumPieces", &result.num_pieces))
  SET_RESULT_IF(ImGui::Checkbox("UseVariablePieceLength", &result.use_variable_piece_length))
  SET_RESULT_IF(ImGui::Checkbox("UseIsectWallOBB", &result.use_isect_wall_obb))
  SET_RESULT_IF(ImGui::Checkbox("AutoExtrude", &result.auto_extrude))
  SET_RESULT_IF(ImGui::Checkbox("AutoProjectInternodes", &result.auto_project_internodes))
  SET_RESULT_IF(ImGui::Checkbox("RandomizeWallScale", &result.randomize_wall_scale))
  SET_RESULT_IF(ImGui::Checkbox("RandomizePieceType", &result.randomize_piece_type))
  SET_RESULT_IF(ImGui::Checkbox("RestrictXLength", &result.restrict_structure_x_length))
  SET_RESULT_IF(default_input_float("Dt", &result.dt))
  SET_RESULT_IF(default_input_float("TargetLength", &result.target_length))
  SET_RESULT_IF(default_input_float("DelayToRecede", &result.delay_to_recede_s))
  SET_RESULT_IF(ImGui::Checkbox("AllowRecede", &result.allow_recede))
  if (ImGui::Button("SetPreset1")) {
    result.set_preset1 = true;
    has_result = true;
  }
  if (ImGui::Button("ResetGrowingStructure")) {
    gui_res.reset_growing_structure = true;
  }
  if (ImGui::Button("ExtrudeGrowingStructure")) {
    gui_res.extrude_growing_structure = true;
  }
  return has_result ? Optional<P>(result) : NullOpt{};
}

Optional<ArchGUIUpdateResult::GridParams> render_grid_params(const DebugArchComponent& component) {
  using P = ArchGUIUpdateResult::GridParams;

  P result;
  result.fib_n = component.params.grid_fib_n;
  result.permit_quad_probability = component.params.grid_permit_quad_probability;
  result.relax_iters = component.params.grid_relax_params.iters;
  result.neighbor_length_scale = component.params.grid_relax_params.neighbor_length_scale;
  result.quad_scale = component.params.grid_relax_params.quad_scale;
  result.grid_projected_terrain_offset = component.params.grid_projected_terrain_offset;
  result.grid_projected_terrain_scale = component.params.grid_projected_terrain_scale;
  result.draw_grid = component.params.draw_projected_grid;
  result.update_enabled = component.params.grid_update_enabled;
  result.set_preset1 = false;
  result.apply_height_map = component.params.apply_height_map_to_grid;

  bool has_result = false;
  SET_RESULT_IF(ImGui::InputInt("FibN", &result.fib_n))
  SET_RESULT_IF(default_input_float("PQuad", &result.permit_quad_probability))
  SET_RESULT_IF(ImGui::InputInt("RelaxIters", &result.relax_iters))
  SET_RESULT_IF(default_input_float("NeighborLengthScale", &result.neighbor_length_scale))
  SET_RESULT_IF(default_input_float("QuadScale", &result.quad_scale))
  SET_RESULT_IF(default_input_float2("ProjectedScale", &result.grid_projected_terrain_scale.x))
  SET_RESULT_IF(default_input_float3("ProjectedOffset", &result.grid_projected_terrain_offset.x))
  SET_RESULT_IF(ImGui::Checkbox("DrawGrid", &result.draw_grid))
  SET_RESULT_IF(ImGui::Checkbox("UpdateEnabled", &result.update_enabled))
  SET_RESULT_IF(ImGui::Checkbox("ApplyHeightMap", &result.apply_height_map))
  if (ImGui::Button("SetPreset1")) {
    result.set_preset1 = true;
    has_result = true;
  }
  return has_result ? Optional<P>(result) : NullOpt{};
}

#undef SET_RESULT_IF

int total_num_projected_internodes(const DebugArchComponent& component) {
  int ct{};
  for (auto& nodes : component.debug_projected_nodes) {
    ct += int(nodes.internodes.size());
  }
  return ct;
}

} //  anon

ArchGUIUpdateResult ArchGUI::render(const DebugArchComponent& arch_component) {
  ArchGUIUpdateResult result;
  ImGui::Begin("ArchGUI");

  uint32_t num_tris = arch_component.params.num_triangles;
  uint32_t num_verts = arch_component.params.num_vertices;
  auto num_nodes = total_num_projected_internodes(arch_component);
  ImGui::Text("%d Triangles", num_tris);
  ImGui::Text("%d Vertices", num_verts);
  ImGui::Text("%d Projected Nodes", num_nodes);

  if (ImGui::TreeNode("CollideThroughHole")) {
    result.collide_through_hole_params = render_collide_through_hole_params(
      arch_component, result);
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("RenderGrowth")) {
    result.render_growth_params = render_render_growth_params(arch_component);
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("StructureGrowth")) {
    if (ImGui::Button("ProjectNodes")) {
      result.need_project_nodes_onto_structure = true;
    }
    if (ImGui::Button("PickTriangle")) {
      result.pick_growing_structure_triangle = true;
    }
    result.structure_growth_params = render_structure_growth_params(arch_component, result);
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("GridParams")) {
    if (ImGui::Button("RemakeGrid")) {
      result.remake_grid = true;
    }
    result.grid_params = render_grid_params(arch_component);
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("Wall")) {
    auto theta = arch_component.params.debug_wall_theta;
    if (ImGui::SliderFloat("Theta", &theta, 0.0f, 2.0f * pif(), "%0.3f")) {
      result.new_theta = theta;
    }

    auto extruded_theta = arch_component.params.extruded_theta;
    if (ImGui::SliderFloat("ExtrudedTheta", &extruded_theta, -pif(), pif(), "%0.3f")) {
      result.new_extruded_theta = extruded_theta;
    }

    auto off = arch_component.params.debug_wall_offset;
    if (ImGui::InputFloat3("Offset", &off.x, "%0.3f", enter_flag())) {
      result.new_offset = off;
    }

    auto ar = arch_component.params.debug_wall_aspect_ratio;
    if (default_input_float("AspectRatio", &ar)) {
      result.new_aspect_ratio = ar;
    }

    auto scale = arch_component.params.debug_wall_scale;
    if (ImGui::InputFloat3("Scale", &scale.x, "%0.3f", enter_flag())) {
      result.new_scale = scale;
    }

    int hole_ind{};
    for (auto& hole : arch_component.wall_holes) {
      auto hole_label = make_tagged_label("Hole", hole_ind);
      if (ImGui::TreeNode(hole_label.c_str())) {
        if (auto new_hole = render_wall_hole(hole)) {
          result.new_holes = arch_component.wall_holes;
          result.new_holes[hole_ind] = new_hole.value();
        }
        ImGui::TreePop();
      }
      hole_ind++;
    }
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("TreeNodeProject")) {
    if (ImGui::SmallButton("PickTriangle")) {
      result.pick_debug_structure_triangle = true;
    }

    int ith_non_adjacent_tri = arch_component.params.ith_non_adjacent_tri;
    if (ImGui::InputInt("IthNonAdjacentTri", &ith_non_adjacent_tri) && ith_non_adjacent_tri >= 0) {
      result.ith_non_adjacent_tri = ith_non_adjacent_tri;
    }

    auto ray_theta1 = float(arch_component.params.debug_ray1_theta);
    if (ImGui::SliderFloat("Ray1Theta", &ray_theta1, -pif(), pif(), "%0.3f")) {
      result.projected_ray1_theta = double(ray_theta1);
    }

    auto ray1_len = float(arch_component.params.debug_ray1_len);
    if (ImGui::SliderFloat("Ray1Len", &ray1_len, 0.01f, 16.0f, "%0.3f")) {
      result.projected_ray1_length = double(ray1_len);
    }

    bool randomize_theta = arch_component.params.randomize_ray1_direction;
    if (ImGui::Checkbox("RandomizeRay1Theta", &randomize_theta)) {
      result.randomize_projected_ray_theta = randomize_theta;
    }

    auto ray_ti = int(arch_component.params.debug_ray_ti);
    if (ImGui::InputInt("RayTi", &ray_ti) && ray_ti >= 0) {
      result.projected_ray_ti = uint32_t(ray_ti);
    }

    bool proj_medial_axis_only = arch_component.params.project_medial_axis_only;
    if (ImGui::Checkbox("ProjectMedialAxisOnly", &proj_medial_axis_only)) {
      result.project_medial_axis_only = proj_medial_axis_only;
    }

    constexpr int num_ray_ti_presets = 2;
    int ray_ti_presets[num_ray_ti_presets] = {2500, 3000};
    int preset_ind{};
    for (int ti : ray_ti_presets) {
      char txt[64];
      if (int ct = std::snprintf(txt, 64, "Ray%d", ti); ct > 0 && ct < 64) {
        if (ImGui::SmallButton(txt)) {
          result.projected_ray_ti = ti;
        }
        if (++preset_ind < num_ray_ti_presets) {
          ImGui::SameLine();
        }
      }
    }

    bool use_min_y_ti = arch_component.params.use_minimum_y_ti;
    if (ImGui::Checkbox("UseMinimumYTi", &use_min_y_ti)) {
      result.use_minimum_y_ti = use_min_y_ti;
    }

    bool prune_isect = arch_component.params.prune_intersecting_tree_nodes;
    if (ImGui::Checkbox("PruneIntersecting", &prune_isect)) {
      result.prune_intersecting_tree_nodes = prune_isect;
    }

    int queue_size = arch_component.params.intersecting_tree_node_queue_size;
    if (ImGui::InputInt("QueueSize", &queue_size, 1, 100, enter_flag())) {
      result.intersecting_tree_node_queue_size = queue_size;
    }

    bool reset_node_diam = arch_component.params.reset_tree_node_diameter;
    if (ImGui::Checkbox("ResetNodeDiameter", &reset_node_diam)) {
      result.reset_tree_node_diameter = reset_node_diam;
    }

    bool constrain_child_node_diameter = arch_component.params.constrain_child_node_diameter;
    if (ImGui::Checkbox("ConstrainChildNodeDiameter", &constrain_child_node_diameter)) {
      result.constrain_child_node_diameter = constrain_child_node_diameter;
    }

    float max_diam = arch_component.params.max_internode_diameter;
    if (default_input_float("MaxDiameter", &max_diam)) {
      result.max_internode_diameter = max_diam;
    }

    bool constrain_diam = arch_component.params.constrain_internode_diameter;
    if (ImGui::Checkbox("ConstrainNodeDiameter", &constrain_diam)) {
      result.constrain_internode_diameter = constrain_diam;
    }

    bool grow_by_instr = arch_component.params.grow_internodes_by_instrument;
    if (ImGui::Checkbox("GrowByInstrument", &grow_by_instr)) {
      result.grow_internodes_by_instrument = grow_by_instr;
    }

    float inode_growth_signal_scale = arch_component.params.internode_growth_signal_scale;
    if (default_input_float("GrowthSignalScale", &inode_growth_signal_scale)) {
      result.internode_growth_signal_scale = inode_growth_signal_scale;
    }

    bool smooth_diam = arch_component.params.smooth_tree_node_diameter;
    if (ImGui::Checkbox("SmoothNodeDiameter", &smooth_diam)) {
      result.smooth_tree_node_diameter = smooth_diam;
    }

    bool smooth_ns = arch_component.params.smooth_tree_node_normals;
    if (ImGui::Checkbox("SmoothNormals", &smooth_ns)) {
      result.smooth_tree_node_normals = smooth_ns;
    }

    bool offset_by_radius = arch_component.params.offset_tree_nodes_by_radius;
    if (ImGui::Checkbox("OffsetTreeNodesByRadius", &offset_by_radius)) {
      result.offset_tree_nodes_by_radius = offset_by_radius;
    }

    int diam_adj_count = arch_component.params.smooth_diameter_adjacent_count;
    if (ImGui::InputInt("SmoothDiameterAdjacentCount", &diam_adj_count)) {
      result.smooth_diameter_adjacent_count = diam_adj_count;
    }

    int norm_adj_count = arch_component.params.smooth_normals_adjacent_count;
    if (ImGui::InputInt("SmoothNormalsAdjacentCount", &norm_adj_count, 1, 100, enter_flag())) {
      result.smooth_normals_adjacent_count = norm_adj_count;
    }

    float diam_power = arch_component.params.node_diameter_power;
    if (ImGui::SliderFloat("NodeDiameterPower", &diam_power, 0.25f, 2.0f)) {
      result.node_diameter_power = diam_power;
    }

    if (ImGui::Button("SetPreset1")) {
      result.set_preset1 = true;
    }

    float leaves_scale = arch_component.params.leaves_scale;
    if (ImGui::SliderFloat("LeavesScale", &leaves_scale, 0.0f, 2.0f)) {
      result.leaves_scale = leaves_scale;
    }

    if (ImGui::Button("RetriggerAxisGrowth")) {
      result.retrigger_axis_growth = true;
    }

    float growth_incr = arch_component.params.axis_growth_incr;
    if (ImGui::SliderFloat("AxisGrowthIncr", &growth_incr, 0.0f, 1.0f)) {
      result.axis_growth_incr = growth_incr;
    }

    ImGui::TreePop();
  }

  if (ImGui::TreeNode("General")) {
    bool draw_bounds = arch_component.params.draw_wall_bounds;
    if (ImGui::Checkbox("DrawWallBounds", &draw_bounds)) {
      result.draw_wall_bounds = draw_bounds;
    }

    bool draw_cubes = arch_component.params.draw_debug_cubes;
    if (ImGui::Checkbox("DrawDebugCubes", &draw_cubes)) {
      result.draw_debug_cubes = draw_cubes;
    }

    bool draw_node_bounds = arch_component.params.draw_tree_node_bounds;
    if (ImGui::Checkbox("DrawNodeBounds", &draw_node_bounds)) {
      result.draw_tree_node_bounds = draw_node_bounds;
    }

    bool draw_proj_res = arch_component.params.draw_project_ray_result;
    if (ImGui::Checkbox("DrawProjectRayResult", &draw_proj_res)) {
      result.draw_project_ray_result = draw_proj_res;
    }

    bool draw_node_normals = arch_component.params.draw_extracted_tree_node_normals;
    if (ImGui::Checkbox("DrawNodeNormals", &draw_node_normals)) {
      result.draw_extracted_tree_node_normals = draw_node_normals;
    }

    bool draw_stem = arch_component.params.draw_stem_drawable;
    if (ImGui::Checkbox("DrawStem", &draw_stem)) {
      result.draw_stem_drawable = draw_stem;
    }

    if (ImGui::Button("RemakeWall")) {
      result.remake_wall = true;
    }

    if (ImGui::Button("ToggleNormals")) {
      result.toggle_normal_visibility = true;
    }

    if (ImGui::Button("ToggleArch")) {
      result.toggle_arch_visibility = true;
    }

    if (ImGui::Button("ToggleDebugNodes")) {
      result.toggle_debug_nodes_visibility = true;
    }

    char text[1024];
    memset(text, 0, 1024);
    if (ImGui::InputText("SaveTriangulation", text, 1024, enter_flag())) {
      result.save_triangulation_file_path = std::string{text};
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
