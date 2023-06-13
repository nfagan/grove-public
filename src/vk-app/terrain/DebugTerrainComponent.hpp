#pragma once

#include "../render/StaticModelRenderer.hpp"
#include "../render/TerrainRenderer.hpp"
#include "../render/ProceduralTreeRootsRenderer.hpp"
#include "../transform/transform_system.hpp"
#include "../bounds/bounds_system.hpp"
#include "../procedural_tree/radius_limiter.hpp"
#include "grove/math/Bounds3.hpp"
#include "grove/math/OBB3.hpp"
#include <string>

namespace grove {

struct TerrainGUIUpdateResult;
class Terrain;

class DebugTerrainComponent {
public:
  struct CubeMarchParams {
    bool active{true};
    bool invert{};
    bool need_recompute{};
    bool use_wall_brush{};
    bool made_perimeter_wall{};
    bool need_increase_wall_height{};
    bool need_decrease_wall_height{};
    int height_index{};
    int cumulative_height_index{};
    bool need_initialize_wall{true};
    bool allow_wall_recede{};
    bool draw_bounds{};
    bool hidden{};
    bool need_clear{};
    float wall_brush_speed{1.0f};
    float wall_brush_circle_scale{0.0f};
    float wall_random_axis_weight{1.0f};
    bool brush_control_by_instrument{true};
    Optional<float> instrument_brush_speed;
    float instrument_brush_circle_frac{};
    float instrument_brush_circle_scale{0.0f};
  };

  struct CubeMarchStats {
    int num_voxel_samples;
    int num_voxel_blocks;
    int num_cube_march_triangles;
    int num_cube_march_vertices;
    int num_cube_march_chunks;
  };

  struct PlaceOnMeshParams {
    bool need_recompute{};
    Vec3f obb3_size{0.5f, 2.0f, 0.5f};
    bool draw_result{true};
    float normal_y_threshold{0.95f};
  };

  struct NodesThroughTerrainParams {
    bool keep_axis{};
    int keep_ith_axis{};
    bool need_update_roots_drawable{};
  };

  struct Model {
    StaticModelRenderer::GeometryHandle geom{};
    StaticModelRenderer::MaterialHandle material{};
    StaticModelRenderer::DrawableHandle drawable{};
    vk::SampledImageManager::Handle image{};
  };

  struct UpdateInfo {
    double real_dt;
    const Bounds3f* tree_aabbs;
    const Vec3f* tree_base_positions;
    int num_tree_aabbs;
    const OBB3f* wall_bounds;
    int num_wall_bounds;
    bounds::BoundsSystem* bounds_system;
    bounds::AccelInstanceHandle accel_handle;
    bounds::RadiusLimiter* radius_limiter;
    StaticModelRenderer& model_renderer;
    const StaticModelRenderer::AddResourceContext& model_renderer_context;
    TerrainRenderer& terrain_renderer;
    const TerrainRenderer::AddResourceContext& terrain_renderer_context;
    ProceduralTreeRootsRenderer& roots_renderer;
    const ProceduralTreeRootsRenderer::AddResourceContext& roots_renderer_context;
    vk::SampledImageManager& sampled_image_manager;
    transform::TransformSystem& tform_system;
    const Terrain& terrain;
  };

  struct AddTransformEditor {
    transform::TransformInstance* inst;
    Vec3f color;
  };

  struct UpdateResult {
    AddTransformEditor add_tform_editors[64];
    int num_add;
    Optional<vk::SampledImageManager::Handle> new_splotch_image;
    Optional<vk::SampledImageManager::Handle> new_ground_color_image;
  };

public:
  UpdateResult update(const UpdateInfo& info);
  void on_gui_update(const TerrainGUIUpdateResult& res);
  CubeMarchStats get_cube_march_stats() const;
  float get_cube_march_editor_radius() const;
  Optional<Vec2f> get_roots_rotation() const;
  void set_brush_speed01(float v);
  void set_brush_direction01(float v);
  int changed_height_direction() const;
  bounds::ElementTag get_terrain_bounds_element_tag() const {
    return bounds_element_tag;
  }

public:
  Optional<std::string> geometry_file_path;
  Optional<std::string> image_file_path;
  Optional<std::string> splotch_image_file_path;
  bool tried_load_splotch_image{};
  Optional<std::string> color_image_file_path;
  bool tried_load_color_image{};
  vk::SampledImageManager::Handle splotch_image{};
  vk::SampledImageManager::Handle ground_color_image{};

  DynamicArray<transform::TransformInstance*, 8> debug_tforms;
  DynamicArray<Model, 8> debug_models;
  int debug_model_index{};

  CubeMarchParams cube_march_params;
  PlaceOnMeshParams place_on_mesh_params;
  NodesThroughTerrainParams nodes_through_terrain_params;
  std::vector<OBB3f> component_bounds;
  bool need_insert_component_bounds{};

  bounds::AccessorID bounds_accessor{bounds::AccessorID::create()};
  bounds::ElementTag bounds_element_tag{bounds::ElementTag::create()};

  bounds::RadiusLimiterElementTag radius_limiter_element_tag{bounds::RadiusLimiterElementTag::create()};
  bounds::RadiusLimiterAggregateID radius_limiter_aggregate_id{bounds::RadiusLimiterAggregateID::create()};
};

}