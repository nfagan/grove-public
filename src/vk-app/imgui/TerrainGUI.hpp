#pragma once

#include "grove/common/Optional.hpp"
#include "grove/math/Vec2.hpp"
#include "grove/math/Vec3.hpp"
#include <string>

namespace grove {

class DebugTerrainComponent;

struct TerrainGUIUpdateResult {
  Optional<std::string> geometry_file_path;
  Optional<std::string> image_file_path;
  Optional<std::string> splotch_image_file_path;
  Optional<std::string> ground_color_image_file_path;
  Optional<std::string> alt_terrain_color_image_file_path;
  Optional<Vec3f> model_scale;
  Optional<Vec3f> model_translation;
  Optional<int> model_index;
  Optional<bool> invert_cube_march_tool;
  Optional<bool> cube_march_editing_active;
  Optional<float> cube_march_editor_radius;
  Optional<bool> cube_march_hidden;
  Optional<bool> cube_march_use_wall_brush;
  Optional<bool> cube_march_control_wall_brush_by_instrument;
  Optional<bool> cube_march_draw_bounds;
  Optional<float> cube_march_wall_brush_speed;
  Optional<float> cube_march_wall_random_axis_weight;
  Optional<float> cube_march_wall_circle_scale;
  Optional<bool> allow_cube_march_wall_recede;
  Optional<Vec3f> mesh_obb3_size;
  Optional<bool> draw_place_on_mesh_result;
  Optional<float> place_on_mesh_normal_y_threshold;
  Optional<Vec2f> debug_roots_rotation;
  Optional<bool> keep_axis;
  Optional<int> keep_ith_axis;
  bool add_model{};
  bool recompute_cube_march_geometry{};
  bool clear_cube_march_geometry{};
  bool recompute_mesh_projected_bounds{};
  bool need_increase_cube_march_wall_height{};
  bool need_decrease_cube_march_wall_height{};
  bool need_reinitialize_cube_march_wall{};
  bool close{};
};

class TerrainGUI {
public:
  TerrainGUIUpdateResult render(const DebugTerrainComponent& component);
};

}