#pragma once

#include "../render/ProceduralTreeRootsRenderer.hpp"
#include "../transform/transform_system.hpp"
#include "radius_limiter.hpp"
#include "../render/SampledImageManager.hpp"
#include "../editor/transform_editor.hpp"
#include "../procedural_tree/components.hpp"
#include "grove/common/Unique.hpp"

namespace grove {

namespace editor {
struct Editor;
}

struct ProceduralTreeRootsGUIUpdateResult;
class WindDisplacement;
class SpatiallyVaryingWind;
class Terrain;

class DebugTreeRootsComponent {
public:
  struct Material1Colors {
    Vec3<uint8_t> c0{31, 79, 61};
    Vec3<uint8_t> c1{138, 255, 187};
    Vec3<uint8_t> c2{0, 90, 0};
    Vec3<uint8_t> c3{0, 100, 0};
  };

  struct Params {
    float diameter_scale{1.0f};
    bool drawable_needs_update{};
//    float growth_rate{1.0f};
    float growth_rate{0.125f};
    float leaf_diameter{0.075f};
    float diameter_power{1.8f};
    bool validate_radius_limiter{true};
    bool need_create_roots{};
    bool need_create_short_tree{};
    int num_roots_create{1};
    bool allow_recede{};
    bool add_roots_at_new_tree_origins{};
    bool need_spawn_axis{};
    bool draw_cube_grid{};
    bool debug_draw_enabled{};
    bool add_roots_at_tform{};
    float attractor_point_scale{0.1f};
    bool camera_position_attractor{};
    int selected_root_index{};
    int selected_node_index{};
    bool draw_node_frames{};
    double p_spawn_lateral{0.1};
    float min_axis_length_spawn_lateral{16.0f};
    float node_length{1.0f};
    bool make_tree{};
    bool need_generate_sample_points_on_nodes{};
    float points_on_nodes_radius_offset{};
    float points_on_nodes_step_size{1.0f};
    Vec3f points_on_nodes_step_axis{0.0f, 1.0f, 0.0f};
    bool points_on_nodes_prefer_entry_up_axis{true};
    float points_on_nodes_leaf_diameter{0.04f};
    float points_on_nodes_diameter_power{1.8f};
    Vec3<uint8_t> points_on_nodes_color{};
    bool smooth_points_on_nodes{true};
    bool wind_disabled{};
    Material1Colors material1_colors{};
    bool need_fit_bounds_around_axis{};
    Vec3f default_root_origin{0.0f, 4.0f, 32.0f};
    float rand_root_origin_span{16.0f};
    bool scale_growth_rate_by_signal{};
    int max_num_nodes_per_roots{512};
    bool prefer_global_p_spawn_lateral{};
  };

  struct InitInfo {
    bounds::RadiusLimiter* radius_limiter;
    bounds::RadiusLimiterElementTag roots_tag;
    const ProceduralTreeRootsRenderer::AddResourceContext& roots_renderer_context;
    ProceduralTreeRootsRenderer& roots_renderer;
    transform::TransformSystem& transform_system;
    vk::SampledImageManager& sampled_image_manager;
    editor::Editor* editor;
  };

  struct InitResult {
    //
  };

  struct UpdateInfo {
    editor::Editor* editor;
    bounds::RadiusLimiter* radius_limiter;
    bounds::RadiusLimiterElementTag roots_tag;
    const ProceduralTreeRootsRenderer::AddResourceContext& roots_renderer_context;
    ProceduralTreeRootsRenderer& roots_renderer;
    double real_dt;
    const Vec3f* newly_created_tree_origins;
    int num_newly_created_trees;
    Vec3f camera_position;
    const SpatiallyVaryingWind& wind;
    const Terrain& terrain;
    const WindDisplacement& wind_displacement;
    const Bounds3f& world_aabb;
  };

public:
  InitResult initialize(const InitInfo& info);
  void update(const UpdateInfo& info);
  void on_gui_update(const ProceduralTreeRootsGUIUpdateResult&);
  float max_radius() const;
  int num_growing() const;
  int num_receding() const;
  int num_root_aggregates() const;
  bool is_root_node_radius_constrained(const bounds::RadiusLimiter* lim, int ri) const;
  bool any_root_nodes_radius_constrained(const bounds::RadiusLimiter* lim) const;
  void set_spectral_fraction(float f01);
  Vec3f get_attractor_point() const;

public:
  Optional<ProceduralTreeRootsRenderer::DrawableHandle> debug_drawable;
  tree::Internodes debug_internodes;
  Params params{};
  Optional<std::string> deserialize_from_file;
  Optional<std::string> serialize_to_file;
  transform::TransformInstance* debug_grid_tform{};
  transform::TransformInstance* debug_attractor_tform{};
  float spectral_fraction{};

  editor::TransformEditorHandle grid_tform_editor{};
  editor::TransformEditorHandle attractor_tform_editor{};
};

}