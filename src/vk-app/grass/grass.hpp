#pragma once

#include "../grass/FrustumGrid.hpp"
#include "../grass/instancing.hpp"
#include "grove/visual/Camera.hpp"

namespace grove {

struct GrassVisualParams {
  Vec3f blade_scale = Vec3f(0.15f, 2.0f, 1.0f);
  Vec3f next_blade_scale = blade_scale;
  float taper_power = 3.0;
  Vec2f near_z_extents{};
  Vec2f near_scale_factors{};
  Vec2f far_z_extents = Vec2f(50.0f, 70.0f);
  Vec2f far_scale_factors = Vec2f(1.0f, 1.0f);
  int num_blade_segments = 5;
};

struct GrassInitParams {
  FrustumGrid::Parameters frustum_grid_params;
  GrassInstanceOptions instance_options;
  GrassVisualParams visual_params;
};

struct Grass {
  FrustumGrid grid;
};

GrassInitParams make_high_lod_grass_low_lod_preset_init_params(const ProjectionInfo& proj_info);
GrassInitParams make_high_lod_grass_init_params(const ProjectionInfo& proj_info);
GrassInitParams make_low_lod_grass_init_params(const ProjectionInfo& proj_info);

}