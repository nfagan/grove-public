#pragma once

#include "grove/common/Optional.hpp"
#include "../procedural_tree/components.hpp"
#include <string>

namespace grove {

class ProceduralTreeComponent;
class ProceduralFlowerOrnamentParticles;

namespace tree {
struct GrowthSystem2;
}

class ProceduralTreeGUI {
public:
  struct GUIUpdateResult {
    bool close{};
    bool make_new_tree{};
    bool add_tree_at_tform_position{};
    bool remake_drawables{};
    bool make_trees_at_origin{};
    Optional<int> prune_selected_axis_index;
    Optional<bool> render_attraction_points;
    Optional<bool> tree_spawn_enabled;
    Optional<bool> render_node_skeleton;
    Optional<float> axis_growth_incr;
    Optional<bool> axis_growth_by_signal;
    Optional<bool> randomize_static_or_proc_leaves;
    Optional<bool> use_static_leaves;
    Optional<bool> disable_static_leaves;
    Optional<bool> disable_foliage_components;
    Optional<bool> use_hemisphere_color_image;
    Optional<bool> randomize_hemisphere_color_images;
    Optional<bool> always_small_proc_leaves;
    Optional<bool> can_trigger_death;
    Optional<int> attraction_points_type;
    Optional<int> spawn_params_type;
    Optional<bool> is_pine;
    Optional<int> foliage_leaves_type;
    Optional<bool> wind_influence_enabled;
    Optional<float> proc_wind_fast_osc_scale;
    Optional<float> static_wind_fast_osc_scale;
    Optional<tree::TreeID> selected_tree;
    Optional<float> signal_axis_growth_scale;
    Optional<float> signal_leaf_growth_scale;
    Optional<int> num_trees_manually_add;
    Optional<Vec3f> tree_origin;
    Optional<float> tree_origin_span;
    Optional<bool> add_flower_patch_after_growing;
    Optional<bool> hide_foliage_drawable_components;
    Optional<Vec3f> deserialized_tree_translation;
    Optional<std::string> serialize_selected_to_file_path;
    Optional<std::string> deserialize_from_file_path;
    Optional<float> resource_spiral_theta;
    Optional<float> resource_spiral_vel;
    Optional<bool> vine_growth_by_signal;
  };

public:
  GUIUpdateResult render(ProceduralTreeComponent& component,
                         const tree::GrowthSystem2* growth_system);

private:
  int attraction_points_type{};
  bool show_tree_stats{};
  bool show_selectable_trees{false};
  int prune_selected_axis_index{};
};

}