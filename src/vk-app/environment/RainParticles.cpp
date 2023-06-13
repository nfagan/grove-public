#include "RainParticles.hpp"
#include "../wind/SpatiallyVaryingWind.hpp"
#include "grove/common/common.hpp"
#include "grove/common/vector_util.hpp"
#include "grove/common/logging.hpp"
#include "grove/math/matrix_transform.hpp"
#include "grove/math/random.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

constexpr double sim_dt() {
  return 1.0 / 60.0;
}

[[maybe_unused]] constexpr const char* logging_id() {
  return "RainParticles";
}

constexpr float wind_force_scale() {
  return 500.0f;
}

constexpr float gravity_force_scale() {
  return 100.0f;
}

Vec3f initial_force() {
  return Vec3f{
    2.0f * urand_11f(),
    -(10.0f + urandf() * 2.5f),
    2.0f * urand_11f()
  } * 100.0f;
}

float particle_mass() {
  return (1.0f + urand_11f() * 0.25f) * 0.5f;
}

float alpha_increment() {
  return 0.005f + urand_11f() * 0.0025f;
}

using GroupParams = RainParticles::GroupParams;
using SimulatedGroup = RainParticles::SimulatedGroup;
using Particle = RainParticles::Particle;
using SimulatedParticle = RainParticles::SimulatedParticle;
using ExpiredParticle = RainParticles::ExpiredParticle;
using ExpiredParticles = RainParticles::ExpiredParticles;
using UpdateInfo = RainParticles::UpdateInfo;

SimulatedParticle make_simulated_particle(const Vec3f& p) {
  RainParticles::ParticleSimulationState state0{};
  state0.position = p;
  state0.velocity = {};
  state0.force = initial_force();
  state0.alpha = 0.0f;

  RainParticles::SimulatedParticle simulated_particle{};
  simulated_particle.mass = particle_mass();
  simulated_particle.alpha_incr = alpha_increment();
  simulated_particle.last = state0;
  simulated_particle.curr = state0;

  return simulated_particle;
}

Particle make_particle(const Vec3f& p) {
  RainParticles::Particle particle{};
  particle.position = p;
  particle.rand01 = urandf();
  return particle;
}

Vec3f randomized_initial_position(const Bounds3f& bounds, const Vec3f& offset) {
  auto span = bounds.size();
  auto v = span * Vec3f{urandf(), urandf(), urandf()} + bounds.min;
  v.y = bounds.max.y; //  start at maximum height.
  return v + offset;
}

SimulatedGroup make_group(RainParticleGroupID id, const GroupParams& params) {
  SimulatedGroup group{};
  group.id = id;
  group.particles.reserve(params.num_particles);
  group.simulated_particles.reserve(params.num_particles);
  group.extent = params.extent;

  for (int i = 0; i < params.num_particles; i++) {
    auto p = randomized_initial_position(params.extent, params.origin);
    group.particles.push_back(make_particle(p));
    group.simulated_particles.push_back(make_simulated_particle(p));
  }

  return group;
}

void simulate_group(SimulatedGroup& group, const UpdateInfo& info) {
  const double dt_scale = info.dt_scale;
  const auto& wind = info.wind;
  const auto& origin = info.origin;

  const double dt = sim_dt() * dt_scale;
  const auto dt2 = dt * dt;

  int particle_ind{};
  for (auto& particle : group.simulated_particles) {
    particle.last = particle.curr;

    auto& state = particle.curr;
    const auto f_wind_xz = wind.wind_force(Vec2f{state.position.x, state.position.z});
    const auto f_wind = Vec3f{f_wind_xz.x, 0.0f, f_wind_xz.y};
    const auto f_g = Vec3f{0.0f, -9.8f, 0.0f};
    const auto f = (f_wind * wind_force_scale() + f_g * gravity_force_scale()) + state.force;

    const auto m = particle.mass;
    auto p = state.position + state.velocity * float(dt) + 0.5f * f / m * float(dt2);

    state.velocity = p - state.position;
    state.position = p;

    for (int i = 0; i < 3; i++) {
      auto pf = state.force[i];
      const auto scl = 256.0f * float(dt);

      if (pf < 0.0f) {
        pf = std::min(pf + scl, 0.0f);
      } else {
        pf = std::max(pf - scl, 0.0f);
      }

      state.force[i] = pf;
    }

    if (state.position.y < origin.y - 2.0f) {
      //  Respawn.
      auto expired_position = state.position;
      auto init_p = randomized_initial_position(group.extent, origin);
      particle = make_simulated_particle(init_p);

      auto& group_particle = group.particles[particle_ind];
      group_particle = make_particle(init_p);
      group_particle.expired_position = expired_position;
      group_particle.expired = true;
    }

    particle_ind++;
  }
}

} //  anon

RainParticles::UpdateResult RainParticles::update(const UpdateInfo& update_info) {
  UpdateResult result{};

  expired_particles.clear();
  simulation_timer.on_frame_entry(update_info.real_dt);
  Stopwatch abort_guard;

  while (simulation_timer.should_proceed(sim_dt())) {
    for (auto& group : groups) {
      simulate_group(group, update_info);
    }
    if (simulation_timer.on_after_simulate_check_abort(sim_dt(), abort_guard, sim_dt() * 0.1)) {
      GROVE_LOG_WARNING_CAPTURE_META("Simulation aborted early.", logging_id());
      break;
    }
  }

  const auto time_alpha = simulation_timer.get_accumulated_time() / sim_dt();

  for (auto& group : groups) {
    auto& simulated_particles = group.simulated_particles;
    auto& particles = group.particles;

    for (int i = 0; i < int(particles.size()); i++) {
      const auto& sim_particle = simulated_particles[i];
      auto& sim_last = sim_particle.last;
      auto& sim_curr = sim_particle.curr;
      auto& particle = particles[i];

      particle.position = lerp(float(time_alpha), sim_last.position, sim_curr.position);
      particle.velocity = lerp(float(time_alpha), sim_last.velocity, sim_curr.velocity);
      particle.alpha = lerp(float(time_alpha), sim_last.alpha, sim_curr.alpha);

      if (particle.expired) {
        particle.expired = false;
        ExpiredParticle expired_particle{};
        expired_particle.group_id = group.id;
        expired_particle.position = particle.expired_position;
        expired_particles.push_back(expired_particle);
      }
    }
  }

  result.expired_particles = make_iterator_array_view<ExpiredParticle>(expired_particles);
  return result;
}

RainParticleGroupID RainParticles::push_group(const GroupParams& params) {
  RainParticleGroupID handle{next_group_id++};
  groups.push_back(make_group(handle, params));
  return handle;
}

const SimulatedGroup* RainParticles::get_group(RainParticleGroupID id) const {
  auto it = std::find_if(groups.begin(), groups.end(), [id](auto& group) {
    return group.id == id;
  });
  return it == groups.end() ? nullptr : &*it;
}

GROVE_NAMESPACE_END
