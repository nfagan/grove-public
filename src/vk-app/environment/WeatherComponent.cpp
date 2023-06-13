#include "WeatherComponent.hpp"
#include "../imgui/WeatherGUI.hpp"
#include "grove/common/common.hpp"
#include "grove/visual/Camera.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

struct Config {
  static constexpr float rain_span_xz = 64.0f;
  static constexpr float rain_span_y = 32.0f;
  static constexpr int num_rain_particles = 256;
};

float particle_alpha_scale_from_status(const weather::Status& status) {
  float weather_scale;
  if (status.current == weather::State::Overcast) {
    weather_scale = 1.0f - status.frac_next;
  } else {
    weather_scale = status.frac_next;
  }
  return std::pow(weather_scale, 4.0f);
}

} //  anon

void WeatherComponent::initialize(const InitInfo& info) {
  RainParticles::GroupParams group_params{};
  group_params.extent = Bounds3f{
    Vec3f{-Config::rain_span_xz, 0.0f, -Config::rain_span_xz},
    Vec3f{Config::rain_span_xz, Config::rain_span_y, Config::rain_span_xz}
  };
  group_params.origin = {};
  group_params.num_particles = Config::num_rain_particles;
  debug_particle_group_id = rain_particles.push_group(group_params);
  if (debug_particle_group_id) {
    auto* group = rain_particles.get_group(debug_particle_group_id.value());
    auto drawable = info.rain_particle_renderer.create_drawable(
      info.context,
      uint32_t(group->particles.size()));
    if (drawable) {
      rain_particle_drawable = drawable.value();
    }
  }
  reserve_soil_deposits.resize(Config::num_rain_particles);
}

WeatherComponent::UpdateResult WeatherComponent::update(const UpdateInfo& info) {
  UpdateResult result{};
  result.weather_status = weather_system.update();
  const auto& ws = result.weather_status;

  auto particle_update_res = rain_particles.update({
    info.wind,
    info.player_position,
    info.real_dt,
    params.rain_particle_dt_scale
  });

  const bool is_overcast = ws.current == weather::State::Overcast;
  if (is_overcast || ws.frac_next > 0.0f) {
    const int num_deposits = std::min(
      int(reserve_soil_deposits.size()),
      int(particle_update_res.expired_particles.size()));
    const auto part_strength = 0.25f * (is_overcast ? 1.0f - ws.frac_next : ws.frac_next);

    for (int i = 0; i < num_deposits; i++) {
      auto expired_p = particle_update_res.expired_particles[i].position;
      auto& deposit = reserve_soil_deposits[i];
      deposit.position = Vec2f{expired_p.x, expired_p.z};
      deposit.radius = 4.0f;
      deposit.amount = Vec3f{part_strength};
    }
    result.soil_deposits = ArrayView{
      reserve_soil_deposits.data(),
      reserve_soil_deposits.data() + num_deposits};
  }

  update_rain_particle_renderer(info, ws);
  return result;
}

void WeatherComponent::update_rain_particle_renderer(const UpdateInfo& info,
                                                     const weather::Status& weather_status) {
  if (rain_particle_drawable && debug_particle_group_id) {
    auto* group = rain_particles.get_group(debug_particle_group_id.value());
    info.renderer.set_data(
      rain_particle_drawable.value(), group->particles, info.camera.get_view());

    float rain_particle_alpha_scale = particle_alpha_scale_from_status(weather_status);
    if (params.override_weather_control) {
      rain_particle_alpha_scale = params.manual_rain_particle_alpha_scale;
    } else {
      rain_particle_alpha_scale *= params.rain_particle_alpha_scale;
    }

    auto& render_params = info.renderer.get_render_params();
    render_params.global_alpha_scale = rain_particle_alpha_scale;
    render_params.global_particle_scale = params.rain_particle_scale;
  }
}

void WeatherComponent::on_gui_update(const WeatherGUIUpdateResult& update_res) {
  if (update_res.set_sunny) {
    weather_system.set_immediate_state(weather::State::Sunny);
  }
  if (update_res.set_overcast) {
    weather_system.set_immediate_state(weather::State::Overcast);
  }
  if (update_res.update_enabled) {
    weather_system.set_update_enabled(update_res.update_enabled.value());
  }
  if (update_res.set_frac_next) {
    weather_system.set_frac_next_state(update_res.set_frac_next.value());
  }
  if (update_res.immediate_transition) {
    weather_system.begin_transition();
  }
  if (update_res.rain_alpha_scale) {
    params.rain_particle_alpha_scale = update_res.rain_alpha_scale.value();
    params.override_weather_control = false;
  }
  if (update_res.manual_rain_alpha_scale) {
    params.manual_rain_particle_alpha_scale = update_res.manual_rain_alpha_scale.value();
    params.override_weather_control = true;
  }
}

GROVE_NAMESPACE_END
