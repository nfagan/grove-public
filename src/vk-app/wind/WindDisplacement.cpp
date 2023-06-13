#include "WindDisplacement.hpp"
#include "SpatiallyVaryingWind.hpp"
#include "grove/math/util.hpp"
#include "grove/common/common.hpp"
#include "grove/common/profile.hpp"
#include "grove/common/logging.hpp"
#include "grove/math/random.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using Samples = WindDisplacement::Samples;

inline Vec2f world_rest_position(int i, int j, int dim, const Vec2f& p0, const Vec2f& span) {
  auto fz = float(j) / float(dim);
  auto fx = float(i) / float(dim);
  Vec2f t{fx, fz};
  return span * t + p0;
}

void simulate(const SpatiallyVaryingWind& wind, Samples& samples, int dim,
              const Vec2f& p0, const Vec2f& span, double dt) {
  const auto dt2 = dt * dt;

  for (int i = 0; i < dim; i++) {
    for (int j = 0; j < dim; j++) {
      auto ind = i * dim + j;
      auto world_rest_p = world_rest_position(i, j, dim, p0, span);

#if !GROVE_DAMPED_SPRING_TIP_DISPLACEMENT
      auto f_wind = wind.wind_force(world_rest_p) * 128.0f;
      auto& sample = samples[ind];

      auto x = sample.position - world_rest_p;
      auto f_spring = -sample.k * x;
      auto f = f_wind + f_spring;
      const auto a = 0.5f / sample.m * f;
      auto new_p = sample.position + sample.velocity * float(dt) + a * float(dt2);
      auto new_vel = new_p - sample.position;

      sample.velocity = new_vel;
      sample.position = new_p;
#else
      auto f_wind = wind.wind_force(world_rest_p) * 128.0f;
      auto& sample = samples[ind];

      auto w0 = sample.w0;
      auto w02 = w0 * w0;
      auto v = -2.0f * sample.zeta * w0 * sample.velocity * float(dt);
      auto x = -w02 * (sample.position - world_rest_p);

      auto m = sample.k / w02;
      auto at = f_wind / m + x + v;

      sample.velocity += at * float(dt2);
      sample.position += sample.velocity * float(dt);
#endif
    }
  }
}

} //  anon

WindDisplacement::WindDisplacement() :
  dim{64},
  displacement{std::make_unique<Vec2f[]>(dim * dim)},
  samples_prev{std::make_unique<SamplePoint[]>(dim * dim)},
  samples_curr{std::make_unique<SamplePoint[]>(dim * dim)} {
  //
}

float WindDisplacement::approx_gust_magnitude() const {
  return 0.3f;
}

float WindDisplacement::approx_idle_magnitude() const {
  return 0.1f;
}

void WindDisplacement::initialize(const SpatiallyVaryingWind& wind) {
  const auto& world_bound = wind.world_bound();

  Vec2f p0{world_bound.min.x, world_bound.min.z};
  Vec2f p1{world_bound.max.x, world_bound.max.z};
  auto span = p1 - p0;

  for (int i = 0; i < dim; i++) {
    for (int j = 0; j < dim; j++) {
      auto ind = i * dim + j;
      auto world_rest_p = world_rest_position(i, j, dim, p0, span);
      auto& sample = samples_curr[ind];
      sample.position = world_rest_p;

#if GROVE_DAMPED_SPRING_TIP_DISPLACEMENT
      sample.k = 1024.0f;
      sample.w0 = 60.0f;
      sample.zeta = 50.0f;

      sample.k += urand_11f() * 128.0f;
      sample.w0 += urand_11f() * 10.0f;
      sample.zeta += urand_11f() * 5.0f;
#endif
    }
  }
}

void WindDisplacement::update(const SpatiallyVaryingWind& wind, double real_dt) {
  auto profiler = GROVE_PROFILE_SCOPE_TIC_TOC("WindDisplacement/update");

  const auto& world_bound = wind.world_bound();
  Vec2f p0{world_bound.min.x, world_bound.min.z};
  Vec2f p1{world_bound.max.x, world_bound.max.z};
  auto span = p1 - p0;

  const auto sim_dt = 1.0 / 60.0;

  simulation_timer.on_frame_entry(real_dt);
  Stopwatch abort_guard;

  while (simulation_timer.should_proceed(sim_dt)) {
    std::copy(samples_curr.get(), samples_curr.get() + dim * dim, samples_prev.get());
    simulate(wind, samples_curr, dim, p0, span, sim_dt);

    if (simulation_timer.on_after_simulate_check_abort(sim_dt, abort_guard, sim_dt * 0.5)) {
      GROVE_LOG_WARNING_CAPTURE_META("Wind displacement aborted early.", "WindDisplacement");
      break;
    }
  }

  auto time_alpha = simulation_timer.get_accumulated_time() / sim_dt;

  for (int i = 0; i < dim; i++) {
    for (int j = 0; j < dim; j++) {
      auto ind = i * dim + j;
      auto world_rest_p = world_rest_position(i, j, dim, p0, span);

      auto displace_last = samples_prev[ind].position - world_rest_p;
      auto displace_curr = samples_curr[ind].position - world_rest_p;

      displacement[ind] = lerp(float(time_alpha), displace_last, displace_curr);
    }
  }
}

Vec2f WindDisplacement::evaluate(const Vec2f& frac_p) const {
  int r = clamp(int(frac_p.y * float(dim)), 0, dim-1);
  int c = clamp(int(frac_p.x * float(dim)), 0, dim-1);
  auto ind = c * dim + r;
  return displacement[ind];
}

GROVE_NAMESPACE_END
