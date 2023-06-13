#include "grass.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

GrassInitParams make_low_lod_grass_init_params(const ProjectionInfo& proj_info) {
  GrassInitParams result{};

  FrustumGrid::MatchCameraParameters camera_params;
  camera_params.aspect_ratio = proj_info.aspect_ratio;
  camera_params.field_of_view = proj_info.fov_y;
  camera_params.num_cells = 2800;
  camera_params.custom_data_size = false;

  camera_params.z_offset = 40.0f;
  camera_params.z_extent = 200.0f;

  result.frustum_grid_params = FrustumGrid::Parameters(camera_params);
  result.frustum_grid_params.cell_size = Vec2f(4.0f);
  result.frustum_grid_params.alpha_rise_factor = 1.0f;
  result.frustum_grid_params.alpha_decay_factor = 1.0f;

  result.instance_options.density = 4.0f;
  result.instance_options.max_num_instances = 30000;
  result.instance_options.placement_policy = InstancePlacementPolicy::GoldenRatio;
  result.instance_options.placement_offset = 0.25f;
  result.instance_options.displacement_magnitude = 0.075f * 4.0f;

  result.visual_params.near_z_extents = Vec2f(40.0f, 50.0f);
  result.visual_params.near_scale_factors = Vec2f(0.8f, 1.1f);
  result.visual_params.far_z_extents = Vec2f(184.0f, 234.0f);
  result.visual_params.far_scale_factors = Vec2f(1.0f, 0.0f);

  return result;
}

GrassInitParams make_high_lod_grass_init_params(const ProjectionInfo& proj_info) {
  FrustumGrid::MatchCameraParameters camera_params;

  camera_params.aspect_ratio = proj_info.aspect_ratio;
  camera_params.field_of_view = proj_info.fov_y;
  camera_params.num_cells = 290;
  camera_params.custom_data_size = false;

  camera_params.z_offset = 0.0f;
  camera_params.z_extent = 70.0f;

  FrustumGrid::Parameters frust_params(camera_params);
  frust_params.cell_size = Vec2f(4.0f);
  frust_params.alpha_rise_factor = 1.0f;
  frust_params.alpha_decay_factor = 1.0f;
  frust_params.mark_available_if_behind_camera = false;

  GrassInstanceOptions instance_options{};
  instance_options.density = 16.0f;
  instance_options.next_density = 0.1f;
  instance_options.max_num_instances = 20000;
  instance_options.placement_policy = InstancePlacementPolicy::AlternatingOffsets;

  GrassVisualParams visual_params{};
  visual_params.next_blade_scale = Vec3f(0.25f, 3.0f, 1.0f);
  visual_params.far_z_extents = Vec2f(56.0f, 70.0f);
  visual_params.far_scale_factors = Vec2f(1.0f, 0.0f);

  GrassInitParams result{};
  result.instance_options = instance_options;
  result.frustum_grid_params = frust_params;
  result.visual_params = visual_params;
  return result;
}

GrassInitParams make_high_lod_grass_low_lod_preset_init_params(const ProjectionInfo& proj_info) {
  auto params = make_high_lod_grass_init_params(proj_info);
  params.visual_params.far_z_extents = Vec2f{60.0f, 70.0f};
  params.instance_options.max_num_instances = 8000;
  return params;
}

GROVE_NAMESPACE_END
