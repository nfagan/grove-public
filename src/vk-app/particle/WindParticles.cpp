#include "WindParticles.hpp"
#include "grove/math/random.hpp"
#include "grove/math/constants.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

constexpr float particle_scale() {
  return 0.05f;
}

constexpr float alpha_increment_amount() {
  return 0.01f;
}

constexpr float wind_vel_scale() {
  return 0.1f;
}

} //  anon

void WindParticles::update(const Vec3f& player_pos, const Vec2f& wind_vel) {
  auto dt = stopwatch.delta_update().count();
  auto dt_scale = float(dt / (1.0 / 60.0));
  auto sample_rate = dt == 0.0 ? 60.0 : 1.0 / dt;

  for (int i = 0; i < int(instance_data.size()); i++) {
    auto& instance = instance_data[i];
    auto& meta = meta_data[i];

    meta.lfo0.set_sample_rate(sample_rate);

    auto lfo0_val = meta.lfo0.tick();
    auto pos_bias = lfo0_val * Vec3f{0.01f};
    auto alpha_bias = lfo0_val * 0.005f;
    float rot_bias = lfo0_val * 0.01f;

    instance.position.x += wind_vel.x * wind_vel_scale() * dt_scale;
    instance.position.z += wind_vel.y * wind_vel_scale() * dt_scale;
    instance.position += pos_bias;

    instance.rotation_scale.y += meta.alpha_increment * dt_scale + alpha_bias;
    instance.rotation_scale.x += meta.rot_y_increment * dt_scale + rot_bias;
    instance.rotation_scale.x = std::fmod(instance.rotation_scale.x, pif());

    if (instance.rotation_scale.y < 0.0f) {
      meta.alpha_increment = alpha_increment_amount();
      instance.rotation_scale.y = 0.0f;
      instance.position = meta.initial_position + player_pos;

    } else if (instance.rotation_scale.y > 1.0f) {
      meta.alpha_increment = -alpha_increment_amount();
      instance.rotation_scale.y = 1.0f;
    }
  }
}

void WindParticles::initialize(int num_particles) {
  const auto xz_span = 256.0f;
  const auto y_span = 64.0f;

  for (int i = 0; i < num_particles; i++) {
    instance_data.emplace_back();
    meta_data.emplace_back();

    auto p = Vec3f{urandf() * xz_span - xz_span * 0.5f,
                   urandf() * y_span,
                   urandf() * xz_span - xz_span * 0.5f};

    auto& meta = meta_data.back();
    meta.alpha_increment = alpha_increment_amount() * (urandf() > 0.5f ? 1.0f : -1.0f);
    meta.initial_position = p;
//    meta.rot_y_increment = urandf() * 0.01f;
    meta.rot_y_increment = 0.0f;
    meta.lfo0.set_frequency(urand() * 0.1);

    auto& instance = instance_data.back();
    instance.position = p;
    instance.rotation_scale = Vec3f{urandf() * pif(), urandf(), particle_scale()};
  }
}

GROVE_NAMESPACE_END
