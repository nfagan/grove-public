#include "GrassComponent.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include "../terrain/weather.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

[[maybe_unused]] constexpr const char* logging_id() {
  return "GrassComponent";
}

} //  anon

void GrassComponent::initialize(const InitInfo& init_info) {
  {
#if 0
    high_lod_init_params = make_high_lod_grass_init_params(init_info.camera.get_projection_info());
#else
    high_lod_init_params = make_high_lod_grass_low_lod_preset_init_params(
      init_info.camera.get_projection_info());
#endif
    high_lod_grass.grid = FrustumGrid{high_lod_init_params.frustum_grid_params};
    high_lod_grass_data_updated = true;
  }
  {
    low_lod_init_params = make_low_lod_grass_init_params(init_info.camera.get_projection_info());
    low_lod_grass.grid = FrustumGrid{low_lod_init_params.frustum_grid_params};
    low_lod_grass_data_updated = true;
  }
}

void GrassComponent::begin_frame(const BeginFrameInfo& info) {
  if (high_lod_grass_data_updated) {
    GROVE_LOG_INFO_CAPTURE_META("Updating high lod buffer.", logging_id());
    auto grass_instance_data = make_frustum_grid_instance_data(
      high_lod_grass.grid, high_lod_init_params.instance_options);
    info.renderer.set_high_lod_params(high_lod_init_params.visual_params);
    info.renderer.set_high_lod_data(
      info.set_data_context,
      grass_instance_data,
      high_lod_grass.grid.get_data());
    high_lod_grass_data_updated = false;
  }

  if (low_lod_grass_data_updated) {
    GROVE_LOG_INFO_CAPTURE_META("Updating low lod buffer.", logging_id());
    auto grass_instance_data = make_frustum_grid_instance_data(
      low_lod_grass.grid, low_lod_init_params.instance_options);
    info.renderer.set_low_lod_params(low_lod_init_params.visual_params);
    info.renderer.set_low_lod_data(
      info.set_data_context,
      grass_instance_data,
      low_lod_grass.grid.get_data());
    low_lod_grass_data_updated = false;
  }

  info.renderer.begin_frame_set_high_lod_grid_data(
    info.set_data_context,
    high_lod_grass.grid);
  info.renderer.begin_frame_set_low_lod_grid_data(
    info.set_data_context,
    low_lod_grass.grid);
}

GrassComponent::UpdateResult GrassComponent::update(const UpdateInfo& update_info) {
  UpdateResult result{};
  high_lod_grass.grid.update(
    update_info.camera, update_info.follow_distance, update_info.player_position);
  low_lod_grass.grid.update(
    update_info.camera, update_info.follow_distance, update_info.player_position);
#if 0
  if (stopwatch.delta().count() >= 5.0) {
    stopwatch.reset();
    high_lod_grass_data_updated = true;
    low_lod_grass_data_updated = true;
  }
#endif
  auto render_params = weather::terrain_render_params_from_status(update_info.weather_status);
  result.global_color_scale = render_params.global_color_scale;
  result.min_shadow = render_params.min_shadow;
  result.frac_global_color_scale = render_params.frac_global_color_scale;
  return result;
}

GROVE_NAMESPACE_END
