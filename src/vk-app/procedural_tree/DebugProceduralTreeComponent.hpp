#pragma once

#include "../render/ProceduralFlowerStemRenderer.hpp"
#include "../render/ProceduralTreeRootsRenderer.hpp"
#include "../render/ArchRenderer.hpp"
#include "../render/foliage_occlusion.hpp"
#include "../render/render_tree_leaves.hpp"
#include "../render/frustum_cull_data.hpp"
#include "../render/foliage_drawable_components.hpp"
#include "../render/branch_node_drawable_components.hpp"
#include "../procedural_flower/ProceduralFlowerOrnamentParticles.hpp"
#include "tree_message_system.hpp"
#include "grove/visual/Camera.hpp"

namespace grove {

class SpatiallyVaryingWind;
class Terrain;
class ProceduralTreeComponent;

namespace bounds {
struct RadiusLimiter;
}

namespace tree {
struct RenderTreeSystem;
struct VineSystem;
struct RootsSystem;
struct ResourceSpiralAroundNodesSystem;
}

class DebugProceduralTreeComponent {
public:
  struct InitInfo {
    const ProceduralFlowerStemRenderer::AddResourceContext& stem_create_context;
    const ArchRenderer::AddResourceContext& arch_renderer_context;
    ArchRenderer& arch_renderer;
    ProceduralFlowerStemRenderer& proc_flower_stem_renderer;
    const Terrain& terrain;
  };
  struct UpdateInfo {
    const ProceduralTreeRootsRenderer::AddResourceContext& roots_renderer_context;
    ProceduralTreeRootsRenderer& proc_roots_renderer;
    const SpatiallyVaryingWind& wind;
    ProceduralTreeComponent& proc_tree_component;
    tree::TreeMessageSystem* tree_message_system;
    tree::VineSystem* vine_system;
    const tree::TreeSystem* tree_system;
    const tree::RenderTreeSystem* render_tree_system;
    tree::RenderBranchNodesData* render_branch_nodes_data;
    const bounds::RadiusLimiter* radius_limiter;  //  may be null
    const tree::RootsSystem* roots_system;
    const bounds::Accel* tree_bounds_accel;
    tree::ResourceSpiralAroundNodesSystem* resource_spiral_sys;
    const Camera& camera;
    const Ray& mouse_ray;
    double real_dt;
  };
  struct UpdateResult {
    bool occlusion_system_data_structure_modified;
    bool occlusion_system_clusters_modified;
    Optional<bool> set_tree_leaves_renderer_enabled;
  };
  struct FoliageInstanceParams {
    int n;
    float translation_log_min_x;
    float translation_log_max_x;
    float translation_step_power;
    float translation_step_spread_scale;
    float translation_x_scale;
    float translation_y_scale;
    float rand_z_rotation_scale;
    float curl_scale;
    float global_scale;
    bool only_one_instance;
  };
  struct ExperimentalFoliageDrawable {
    Optional<foliage::TreeLeavesDrawableHandle> leaves_drawable;
    Optional<cull::FrustumCullGroupHandle> cull_group_handle;
    Optional<foliage_occlusion::ClusterGroupHandle> occlusion_cluster_group_handle;
    Optional<foliage::FoliageDrawableComponents> foliage_drawable_components;
  };
  struct GrowthOnNodesParams {
    Vec3f line_color{0.427f, 0.625f, 0.412f};
    bool draw_point_cubes{};
    int method{1};
    bool need_recompute{};
    int ith_source{};
    std::vector<std::vector<Vec3f>> sample_points;
    Vec3f source_p{};
    Vec3f target_p{};
    int spiral_init_ni{};
    float spiral_step_size{0.1f};
    float spiral_step_size_randomness{};
    float spiral_theta{0.7f};
    float spiral_branch_theta{0.65f};
    float spiral_theta_randomness{};
    float spiral_n_off{0.1f};
    bool spiral_randomize_initial_position{};
    int spiral_downsample_interval{};
    int spiral_branch_entry_index{18};
    bool spiral_disable_node_intersect_check{};
    float last_compute_time_ms{};
    float growth_rate_scale{1.0f};
    float vine_radius{0.04f};
  };

public:
  ~DebugProceduralTreeComponent();
  void initialize(const InitInfo& info);
  [[nodiscard]] UpdateResult update(const UpdateInfo& info);
  void render_gui(const tree::VineSystem* vine_system);
  foliage_occlusion::FoliageOcclusionSystem* get_foliage_occlusion_system() const {
    assert(debug_foliage_lod_system);
    return debug_foliage_lod_system;
  }

public:
  struct MessageParticle {
    msg::MessageID associated_message;
    Vec3f canonical_offset;
    Vec3f current_offset;
    Vec3f position;
    Vec2f rotation;
    float rot_osc_phase;
    float osc_phase;
    float osc_freq;
    float lerp_speed;
    float scale;
  };

  struct ActiveMessage {
    msg::MessageID id;
  };

  template <typename T>
  using TreeIDMap = std::unordered_map<tree::TreeID, T, tree::TreeID::Hash>;
  using BranchRenderGrowthContexts = TreeIDMap<tree::RenderAxisGrowthContext>;

  BranchRenderGrowthContexts render_growth_contexts;
  std::vector<Vec2f> petal_transform_dirs;
  Stopwatch debug_flower_growth_stopwatch;
  Optional<ArchRenderer::DrawableHandle> tree_mesh_drawable;

  DynamicArray<ActiveMessage, 4> active_messages;
  std::vector<MessageParticle> message_particles;

  FoliageInstanceParams debug_foliage_instance_params;
  foliage::FoliageDistributionStrategy foliage_distribution_strategy{};
  std::unordered_map<
    tree::TreeID,
    ExperimentalFoliageDrawable,
    tree::TreeID::Hash> debug_foliage_drawables;
  Optional<ProceduralTreeRootsRenderer::DrawableHandle> debug_foliage_roots_drawable;
  int foliage_leaf_image_index{};
  int foliage_hemisphere_color_image_index{3};
  bool need_remake_foliage_drawables{};
  bool foliage_hidden{};
  bool foliage_shadow_disabled{};
  bool foliage_alpha_test_disabled{};

  ProjectionInfo camera_projection_info{};
  Mat4f camera_view{1.0f};
  Vec3f camera_position{};
  bool update_debug_frustum{true};
  float far_plane_distance{256.0f};
  bool draw_debug_frustum_components{};
  Vec3f cube_position{};
  Vec3f cube_size{1.0f};
  bool cube_visible{};
  float wind_strength_scale{1.0f};
  bool wind_disabled{};
  bool render_optimized_foliage{};
  Vec2f optim_fadeout_distances{115.0f, 125.0f};
  Vec2f optim_lod_distances{64.0f, 72.0f};
  bool need_set_leaves_renderer_lod_distances{};
  bool need_set_leaves_renderer_fadeout_distances{};
  float renderer_far_plane_distance{512.0f};
  bool renderer_distance_sort{};
  bool renderer_disable_frustum_cull{};
  bool renderer_disable_optim_update{true};
  bool renderer_enable_occlusion_system_culling{};
  bool renderer_enable_density_system_culling{};
  bool renderer_enable_density_system_fade_in_out{};
  bool renderer_use_index_buffer{true};
  float renderer_shadow_scale{1.0f};
  Optional<bool> renderer_set_always_lod0;
  float renderer_leaf_scale_fraction{1.0f};
  bool need_set_renderer_leaf_scale_fraction{};
  bool override_renderer_leaf_scale{};
  bool disable_renderer_instance_update{};
  bool disable_foliage_update{};
  bool need_randomize_foliage_color{};
  bool need_randomize_foliage_alpha_test_image{};
  bool need_update_foliage_alpha_test_image{true};
  bool need_update_foliage_color_image{true};
  bool allow_multiple_foliage_param_types{};
  Optional<bool> set_tree_leaves_renderer_enabled;
  bool disable_experimental_foliage_drawable_creation{};
  bool disable_auto_foliage_drawable_creation{true};
  bool enable_debug_foliage_drawable_creation{};
  bool enable_foliage_drawable_component_creation{true};

  bool debug_grid_traverse_enabled{};
  Vec3f grid_traverse_grid_dim{16.0f};
  Vec3f grid_traverse_ray_origin{8.0f, 8.0f, 8.0f};
  Vec3f grid_traverse_ray_direction{1.0f, 0.0f, 0.0f};
  int num_grid_steps{16};

  foliage_occlusion::FoliageOcclusionSystem* debug_foliage_lod_system{};
  int foliage_occlusion_cluster_create_interval{2};
  float foliage_lod_cull_distance_threshold{128.0f};
  float foliage_cull_fade_back_in_distance_threshold{128.0f};
  float foliage_min_intersect_area_fraction{0.5f};
  float foliage_tested_instance_scale{1.0f};
  int max_num_foliage_occlusion_steps{8};
  bool debug_draw_foliage_lod_system{};
  foliage_occlusion::CheckOccludedResult latest_occlusion_check_result{};
  bool continuously_check_occlusion{};
  Optional<bool> set_foliage_occlusion_check_fade_in_out;
  bool foliage_occlusion_check_fade_in_out{};
  bool foliage_occlusion_disable_cpu_check{};
  bool foliage_occlusion_only_fade_back_in_below_distance_threshold{};
  bool draw_occluded_instances{true};
  bool draw_cluster_bounds{};
  bool colorize_cluster_instances{};
  bool need_check_foliage_lod_system_occlusion{};
  bool need_clear_foliage_lod_system_culled{};
  int occlusion_system_update_interval{1};
  float occlusion_fade_in_time_scale{1.0f};
  float occlusion_fade_out_time_scale{1.0f};
  float occlusion_cull_time_scale{1.0f};
  Optional<bool> set_foliage_instances_hidden;
  Optional<bool> set_render_foliage_system_instances_hidden;

  std::unordered_map<
    tree::TreeID,
    tree::BranchNodeDrawableComponents,
    tree::TreeID::Hash> debug_branch_node_drawable_components;
  bool disable_debug_branch_node_drawable_components{};

  GrowthOnNodesParams growth_on_nodes_params{};
  bounds::AccessorID bounds_accessor_id{bounds::AccessorID::create()};
};

}