#pragma once

#include "../render/ArchRenderer.hpp"
#include "../render/PointBufferRenderer.hpp"
#include "../render/ProceduralFlowerStemRenderer.hpp"
#include "../transform/transform_system.hpp"
#include "../bounds/bounds_system.hpp"
#include "../procedural_tree/tree_system.hpp"
#include "../procedural_tree/components.hpp"
#include "geometry.hpp"
#include "render.hpp"
#include "ray_project.hpp"
#include "ray_project_adjacency.hpp"
#include "grid.hpp"
#include "structure_growth.hpp"
#include "grove/math/OBB3.hpp"
#include "grove/common/Stopwatch.hpp"

namespace grove {

class ArchGUI;
struct ArchGUIUpdateResult;
class Terrain;

namespace tree {
struct ProjectedNodesSystem;
}

namespace vk {
class SampledImageManager;
}

class DebugArchComponent {
  friend class ArchGUI;
public:
  enum class DebugTreeNodeGrowthState {
    Idle,
    Growing,
    PendingNextAxis
  };

  struct DebugProjectedNodes {
    Optional<ProceduralFlowerStemRenderer::DrawableHandle> stem_drawable;
    std::vector<Vec3f> extracted_normals;
    std::vector<Vec3f> true_normals;
    tree::Internodes internodes;
    std::vector<ProjectRayResultEntry> project_ray_results;
    double ray_theta_offset{};
    tree::RenderAxisGrowthContext axis_growth_context;
    DebugTreeNodeGrowthState growth_state{};
    Stopwatch growth_stopwatch;
    std::vector<int> growing_leaf_instance_indices;
    float growing_leaf_t{};
    Optional<tree::TreeNodeIndex> growing_axis_root;
  };

  struct DebugCube {
    Vec3f p;
    Vec3f s;
    Vec3f color;
  };

  struct CollideThroughHoleParams {
    Vec3f collider_angles{};
    Vec3f wall_angles{};
    int forward_dim{2};
    bool compute_wall{};
    bool with_tree_nodes{true};
    float min_collide_node_diam{0.025f};
    float projected_aabb_scale{1.5f};
    float hole_curl{0.2f};
    bool continuous_compute{};
    bool prune_initially_rejected{true};
    bool reject_all_holes{};
    Vec3f leaf_obb_scale{1.0f};
    Vec3f leaf_obb_offset{};
  };

  struct StructureGrowthParams {
    Vec3f structure_ori{0.0f, 5.5f, 0.0f};
    int num_pieces{10};
    float piece_length{1.0f};
    bool use_variable_piece_length{};
    arch::TryEncirclePointParams encircle_point_params{};
    float target_length{16.0f};
    bool use_isect_wall_obb{};
    bool auto_extrude{};
    bool randomize_wall_scale{true};
    float max_piece_x_length{160.0f};
//    float max_piece_x_length{40.0f};
    bool restrict_structure_x_length{true};
    bool randomize_piece_type{};
    bool auto_project_internodes{true};
//    float delay_to_recede_s{60.0f};
    float delay_to_recede_s{5.0f};
    bool allow_recede{};
  };
  struct RenderGrowthParams {
    float growth_incr{0.025f};
    bool grow_by_instrument{true};
    float instrument_scale{0.15f};
  };

  struct Params {
    float debug_wall_theta{};
    float debug_wall_aspect_ratio{1.0f};
    Vec3f debug_wall_offset{16.0f, 5.5f, 16.0f};
    Vec3f debug_wall_scale{16.0f, 16.0f, 2.0f};
    OBB3f debug_wall_bounds{};
    OBB3f debug_wall_bounds2{};
    float extruded_theta{-0.75f};
    bool draw_wall_bounds{};
    bool draw_debug_cubes{};
    bool draw_project_ray_result{};
    bool draw_tree_node_bounds{};
    bool draw_extracted_tree_node_normals{};
    bool draw_projected_grid{};
    bool draw_stem_drawable{};
    uint32_t num_triangles{};
    uint32_t num_vertices{};
    uint32_t debug_ray_ti{};
    bool use_minimum_y_ti{};
    double debug_ray1_len{4.0};
    double debug_ray1_theta{};
    double debug_ray1_theta_rand_scale{0.4};
    double debug_ray1_len_rand_scale{2.0};
    bool randomize_ray1_direction{true};
    bool project_medial_axis_only{};
    bool prune_intersecting_tree_nodes{true};
    bool reset_tree_node_diameter{true};
    int intersecting_tree_node_queue_size{2};
    bool smooth_tree_node_diameter{};
    bool smooth_tree_node_normals{};
    int smooth_diameter_adjacent_count{2};
    int smooth_normals_adjacent_count{4};
    bool constrain_child_node_diameter{};
    bool offset_tree_nodes_by_radius{};
    float node_diameter_power{1.5f};
    float leaves_scale{0.0f};
    grid::RelaxParams grid_relax_params;
    int grid_fib_n{5};
    float grid_permit_quad_probability{0.5f};
    Vec2f grid_projected_terrain_scale{16.0f};
    Vec3f grid_projected_terrain_offset{0.0f, 2.0f, 0.0f};
    bool grid_update_enabled{true};
    bool apply_height_map_to_grid{true};
    float axis_growth_incr{0.05f};
    bool grow_internodes_by_instrument{};
    float internode_growth_signal_scale{1.0f};
    int ith_non_adjacent_tri{};
    float max_internode_diameter{1.0f};
    bool constrain_internode_diameter{};
  };

  struct InitInfo {
    transform::TransformSystem* transform_system;
    const ArchRenderer::AddResourceContext& arch_renderer_context;
    ArchRenderer& arch_renderer;
    const vk::PointBufferRenderer::AddResourceContext& pb_renderer_context;
    vk::PointBufferRenderer& pb_renderer;
    const ProceduralFlowerStemRenderer::AddResourceContext& stem_renderer_context;
    ProceduralFlowerStemRenderer& stem_renderer;
    vk::SampledImageManager& sampled_image_manager;
    const Terrain& terrain;
  };
  struct UpdateInfo {
    tree::ProjectedNodesSystem* projected_nodes_system;
    const ArchRenderer::AddResourceContext& arch_renderer_context;
    ArchRenderer& arch_renderer;
    const vk::PointBufferRenderer::AddResourceContext& pb_renderer_context;
    vk::PointBufferRenderer& pb_renderer;
    const ProceduralFlowerStemRenderer::AddResourceContext& stem_renderer_context;
    ProceduralFlowerStemRenderer& stem_renderer;
    const Terrain& terrain;
    bounds::ElementTag terrain_bounds_element_tag;
    double real_dt;
    Vec3f centroid_of_tree_origins;
    tree::TreeSystem& tree_system;
    bounds::BoundsSystem& bounds_system;
    bounds::AccelInstanceHandle accel_instance_handle;
    bounds::RadiusLimiter* radius_limiter;
    bounds::RadiusLimiterElementTag roots_radius_limiter_tag;
    const tree::TreeSystem::DeletedInstances& deleted_tree_instances;
    const Ray& mouse_ray;
    bool left_clicked;
  };

  struct InitResult {
    std::vector<transform::TransformInstance*> add_transform_editors;
  };

public:
  InitResult initialize(const InitInfo& info);
  void update(const UpdateInfo& info);
  void on_gui_update(const ArchGUIUpdateResult& gui_res);
  void set_instrument_signal_value(float v);
  void set_instrument_connected();
  int gather_wall_bounds(OBB3f* dst, int max_num_dst);
  const tree::Internodes* get_projection_source_internodes() const {
    return &src_tree_internodes;
  }

public:
  Optional<ArchRenderer::DrawableHandle> arch_drawable;
  Optional<ArchRenderer::GeometryHandle> arch_geometry;
  Optional<vk::PointBufferRenderer::DrawableHandle> debug_normals_drawable;
  std::vector<OBB3f> wall_bounds;
  std::vector<arch::WallHole> wall_holes;
  bool need_update_drawable{};
  bool need_project_nodes_onto_structure{};
  bool need_update_projected_ray{};
  bool need_trigger_axis_growth{};
  bool toggle_normal_visibility{};
  bool toggle_arch_visibility{};
  bool need_retrigger_arch_growth{};
  bool need_retrigger_arch_recede{};
  bool need_reset_structure{};
  bool need_extrude_structure{};
  bool need_compute_extruded_structure_geometry{};
  bool need_toggle_debug_nodes_visible{};
  bool need_pick_growing_structure_triangle{};
  bool need_pick_debug_structure_triangle{};
  arch::GridCache grid_cache;
  arch::WallHoleResult store_wall_hole_result;
  Params params;
  tree::Internodes src_tree_internodes;
  tree::Internodes src_tree_internodes1;

  std::vector<DebugProjectedNodes> debug_projected_nodes;

  std::vector<grid::Quad> grid_quads;
  std::vector<grid::Point> grid_points;
  std::vector<cdt::Triangle> grid_tris;
  std::vector<Vec3f> grid_terrain_projected_points;
  Optional<float> new_leaves_scale;
  StructureGrowthParams structure_growth_params;
  std::vector<OBB3f> debug_structure_growth_bounds;
  RenderGrowthParams render_growth_params;
  std::vector<DebugCube> debug_cubes;

  ray_project::NonAdjacentConnections debug_non_adjacent_connections;
  Optional<uint32_t> picked_growing_structure_triangle;

  CollideThroughHoleParams collide_through_hole_params;
  transform::TransformInstance* obb_isect_wall_tform{};
  transform::TransformInstance* obb_isect_collider_tform{};
  OBB3f isect_wall_obb;
  OBB3f isect_collider_obb;
  vk::PointBufferRenderer::DrawableHandle collide_through_hole_point_drawable;
  ArchRenderer::GeometryHandle collide_through_hole_geometry;
  ArchRenderer::DrawableHandle collide_through_hole_drawable;
  Optional<tree::TreeNodeStore> src_tree_collider;
  tree::Internodes pruned_tree_collider_internodes;
  std::vector<int> pruned_tree_collider_dst_to_src;
  tree::Internodes pruning_src_internodes;
  tree::RenderAxisDeathContext pruned_axis_death_context;
  bool render_pruning{};

  bounds::AccessorID bounds_accessor_id{bounds::AccessorID::create()};
  bounds::ElementTag bounds_arch_element_tag{bounds::ElementTag::create()};

  bounds::RadiusLimiterElementTag arch_radius_limiter_element_tag{bounds::RadiusLimiterElementTag::create()};

  Optional<float> instrument_signal_value;
};

}