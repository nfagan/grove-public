#pragma once

#include "grove/common/Optional.hpp"
#include "grove/math/vector.hpp"
#include "../architecture/geometry.hpp"
#include <string>

namespace grove {

class DebugArchComponent;

struct ArchGUIUpdateResult {
  struct CollideThroughHoleParams {
    Vec3f collider_angles;
    Vec3f wall_angles;
    Vec3f collider_scale;
    Vec3f wall_scale;
    int forward_dim;
    bool with_tree_nodes;
    float min_collide_node_diam;
    float projected_aabb_scale;
    float hole_curl;
    bool continuous_compute;
    bool prune_initially_rejected;
    bool reject_all_holes;
    Vec3f leaf_obb_scale;
    Vec3f leaf_obb_offset;
  };
  struct GridParams {
    int fib_n;
    float permit_quad_probability;
    int relax_iters;
    float neighbor_length_scale;
    float quad_scale;
    Vec2f grid_projected_terrain_scale;
    Vec3f grid_projected_terrain_offset;
    bool draw_grid;
    bool update_enabled;
    bool apply_height_map;
    bool set_preset1;
  };
  struct StructureGrowthParams {
    Vec3f structure_ori;
    int num_pieces;
    float piece_length;
    float dist_attract_until;
    float dist_begin_propel;
    float attract_force_scale;
    float propel_force_scale;
    bool use_variable_piece_length;
    float dt;
    float target_length;
    bool set_preset1;
    bool use_isect_wall_obb;
    bool auto_extrude;
    bool randomize_wall_scale;
    bool randomize_piece_type;
    bool restrict_structure_x_length;
    bool auto_project_internodes;
    float delay_to_recede_s;
    bool allow_recede;
  };
  struct RenderGrowthParams {
    bool retrigger_growth;
    bool retrigger_recede;
    float growth_incr;
    bool grow_by_instrument;
    float instrument_scale;
  };

  Optional<float> new_theta;
  Optional<float> new_extruded_theta;
  Optional<Vec3f> new_scale;
  Optional<Vec3f> new_offset;
  Optional<float> new_aspect_ratio;
  std::vector<arch::WallHole> new_holes;
  Optional<bool> draw_wall_bounds;
  Optional<bool> draw_debug_cubes;
  Optional<bool> draw_tree_node_bounds;
  Optional<bool> draw_project_ray_result;
  Optional<bool> draw_extracted_tree_node_normals;
  Optional<bool> draw_stem_drawable;
  Optional<std::string> save_triangulation_file_path;
  Optional<double> projected_ray1_theta;
  Optional<double> projected_ray1_length;
  Optional<bool> randomize_projected_ray_theta;
  Optional<uint32_t> projected_ray_ti;
  Optional<bool> project_medial_axis_only;
  Optional<bool> use_minimum_y_ti;
  Optional<float> projected_ray_offset_length;
  Optional<bool> prune_intersecting_tree_nodes;
  Optional<int> intersecting_tree_node_queue_size;
  Optional<bool> reset_tree_node_diameter;
  Optional<bool> smooth_tree_node_diameter;
  Optional<bool> smooth_tree_node_normals;
  Optional<bool> offset_tree_nodes_by_radius;
  Optional<int> smooth_diameter_adjacent_count;
  Optional<int> smooth_normals_adjacent_count;
  Optional<bool> constrain_child_node_diameter;
  Optional<float> node_diameter_power;
  Optional<float> leaves_scale;
  Optional<int> ith_non_adjacent_tri;
  Optional<float> max_internode_diameter;
  Optional<bool> constrain_internode_diameter;
  Optional<bool> grow_internodes_by_instrument;
  Optional<float> internode_growth_signal_scale;
  Optional<GridParams> grid_params;
  Optional<StructureGrowthParams> structure_growth_params;
  Optional<RenderGrowthParams> render_growth_params;
  Optional<CollideThroughHoleParams> collide_through_hole_params;
  bool set_preset1{};
  bool toggle_normal_visibility{};
  bool toggle_arch_visibility{};
  bool toggle_debug_nodes_visibility{};
  bool remake_wall{};
  bool remake_grid{};
  bool reset_growing_structure{};
  bool extrude_growing_structure{};
  bool retrigger_axis_growth{};
  bool recompute_collide_through_hole_geometry{};
  bool pick_growing_structure_triangle{};
  bool pick_debug_structure_triangle{};
  bool need_project_nodes_onto_structure{};
  Optional<float> axis_growth_incr;
  bool close{};
};

class ArchGUI {
public:
  ArchGUIUpdateResult render(const DebugArchComponent& arch_component);
};

}