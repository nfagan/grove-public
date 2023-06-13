#pragma once

#include "grove/common/Optional.hpp"
#include "grove/math/Vec3.hpp"
#include <string>

namespace grove {

class DebugTreeRootsComponent;

namespace bounds {
struct RadiusLimiter;
}

struct ProceduralTreeRootsGUIUpdateResult {
  struct Material1Colors {
    Vec3<uint8_t> c0;
    Vec3<uint8_t> c1;
    Vec3<uint8_t> c2;
    Vec3<uint8_t> c3;
  };

  Optional<float> diameter_scale;
  Optional<float> growth_rate;
  Optional<int> selected_root_index;
  Optional<int> selected_node_index;
  Optional<bool> validate_radius_limiter;
  Optional<bool> add_roots_at_transform;
  Optional<float> attractor_point_scale;
  Optional<bool> add_roots_at_new_tree_origins;
  Optional<bool> allow_recede;
  Optional<bool> camera_position_attractor;
  Optional<float> leaf_diameter;
  Optional<float> diameter_power;
  Optional<float> node_length;
  Optional<std::string> deserialize;
  Optional<std::string> serialize;
  Optional<bool> draw_node_frames;
  Optional<float> p_spawn_lateral;
  Optional<float> min_axis_length_spawn_lateral;
  Optional<bool> make_tree;
  Optional<float> points_on_nodes_radius_offset;
  Optional<float> points_on_nodes_step_size;
  Optional<float> points_on_nodes_leaf_diameter;
  Optional<float> points_on_nodes_diameter_power;
  Optional<Vec3<uint8_t>> points_on_nodes_color;
  Optional<bool> points_on_nodes_target_down;
  Optional<bool> points_on_nodes_prefer_entry_up_axis;
  Optional<bool> smooth_points_on_nodes;
  Optional<bool> wind_disabled;
  Optional<bool> scale_growth_rate_by_signal;
  Optional<bool> draw_cube_grid;
  Optional<bool> debug_draw_enabled;
  Optional<Material1Colors> material1_colors;
  Optional<Vec3f> default_root_origin;
  Optional<float> rand_root_origin_span;
  Optional<int> max_num_nodes_per_roots;
  Optional<int> num_roots_create;
  Optional<bool> prefer_global_p_spawn_lateral;
  bool spawn_axis{};
  bool create_roots{};
  bool create_short_tree{};
  bool generate_sample_points{};
  bool close{};
  bool set_points_on_nodes_preset1{};
  bool need_fit_bounds_around_axis{};
};

class ProceduralTreeRootsGUI {
public:
  ProceduralTreeRootsGUIUpdateResult render(const bounds::RadiusLimiter* roots_radius_limiter,
                                            const DebugTreeRootsComponent& debug_component);
};

}