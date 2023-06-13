#include "ProceduralFlowerOrnamentParticles.hpp"
#include "../wind/SpatiallyVaryingWind.hpp"
#include "../procedural_tree/attraction_points.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include "grove/math/util.hpp"
#include "grove/math/random.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using SimulatedParticle = ProceduralFlowerOrnamentParticles::SimulatedParticle;
using Particle = ProceduralFlowerOrnamentParticles::Particle;
using ParticleSimulationState = ProceduralFlowerOrnamentParticles::ParticleSimulationState;
using UpdateInfo = ProceduralFlowerOrnamentParticles::UpdateInfo;
using Handle = ProceduralFlowerOrnamentParticles::Handle;
using SpawnParams = ProceduralFlowerOrnamentParticles::SpawnParams;

[[maybe_unused]] constexpr const char* logging_id() {
  return "ProceduralFlowerOrnamentParticles";
}

constexpr double sim_dt() {
  return 1.0 / 60.0;
}

constexpr float wind_force_scale() {
  return 200.0f;
}

constexpr float gravity_force_scale() {
  return 5.0f;
}

float force_decay_scale() {
  return 192.0f + 64.0f * urand_11f();
}

Vec3f initial_force(float force_scale) {
  return Vec3f{
    2.0f * urand_11f(),
    1.0f + (1.0f * urandf()),
    2.0f * urand_11f()
  } * 100.0f * force_scale;
}

float particle_mass() {
  return (1.0f + urand_11f() * 0.25f) * 0.5f;
}

SimulatedParticle make_simulated_particle(const SpawnParams& params) {
  ParticleSimulationState state0{};
  state0.position = params.origin;
  state0.velocity = {};
  state0.force = initial_force(params.initial_force_scale);

  SimulatedParticle simulated_particle{};
  simulated_particle.mass = particle_mass();
  simulated_particle.last = state0;
  simulated_particle.curr = state0;
  simulated_particle.force_decay_scale = force_decay_scale();
  simulated_particle.wind_force_scale = params.wind_force_scale;
  simulated_particle.aux_force_direction = points::uniform_sphere();

  return simulated_particle;
}

Particle make_particle(Handle handle, const Vec3f& p) {
  Particle particle{};
  particle.handle = handle;
  particle.origin = p;
  particle.position = p;
  return particle;
}

void simulate(std::vector<SimulatedParticle>& simulated_particles, const UpdateInfo& info) {
  const double dt_scale = info.dt_scale;
  const auto& wind = info.wind;

  const double dt = sim_dt() * dt_scale;
  const auto dt2 = dt * dt;

  int particle_ind{};
  for (auto& particle : simulated_particles) {
    particle.last = particle.curr;

    auto& state = particle.curr;
    const auto f_wind_xz = wind.wind_force(Vec2f{state.position.x, state.position.z});
    const auto f_wind = Vec3f{f_wind_xz.x, 0.0f, f_wind_xz.y};
    const auto f_g = Vec3f{0.0f, -9.8f, 0.0f};
    const auto f = f_wind * wind_force_scale() * particle.wind_force_scale +
                   f_g * gravity_force_scale() +
                   state.force +
                   particle.aux_force_direction * wind_force_scale() * particle.aux_force_scale;

    const auto m = particle.mass;
    auto p = state.position + state.velocity * float(dt) + 0.5f * f / m * float(dt2);

    state.velocity = p - state.position;
    state.position = p;

    for (int i = 0; i < 3; i++) {
      auto pf = state.force[i];
      const auto scl = particle.force_decay_scale * float(dt);

      if (pf < 0.0f) {
        pf = std::min(pf + scl, 0.0f);
      } else {
        pf = std::max(pf - scl, 0.0f);
      }

      state.force[i] = pf;
    }

    particle_ind++;
  }
}

} //  anon

void ProceduralFlowerOrnamentParticles::initialize() {
  particles.reserve(128);
  simulated_particles.reserve(128);
}

Handle ProceduralFlowerOrnamentParticles::spawn_particle(const SpawnParams& params) {
  Handle result{next_particle_id++};

  auto part = make_particle(result, params.origin);
  auto sim_part = make_simulated_particle(params);
  particles.push_back(part);
  simulated_particles.push_back(sim_part);

  return result;
}

void ProceduralFlowerOrnamentParticles::remove_particle(Handle handle) {
  if (auto ind = find_particle_index(handle)) {
    particles.erase(particles.begin() + ind.value());
    simulated_particles.erase(simulated_particles.begin() + ind.value());
  } else {
    assert(false);
  }
}

Vec3f ProceduralFlowerOrnamentParticles::get_displacement(Handle handle) {
  if (auto ind = find_particle_index(handle)) {
    auto& part = particles[ind.value()];
    return part.position - part.origin;
  } else {
    assert(false);
    return {};
  }
}

Optional<int> ProceduralFlowerOrnamentParticles::find_particle_index(Handle handle) const {
  auto it = std::find_if(particles.begin(), particles.end(), [handle](const Particle& part) {
    return part.handle == handle;
  });
  if (it == particles.end()) {
    return NullOpt{};
  } else {
    return Optional<int>(int(it - particles.begin()));
  }
}

void ProceduralFlowerOrnamentParticles::set_auxiliary_force_scale(Handle handle, float scl) {
  if (auto ind = find_particle_index(handle)) {
    auto& sim_part = simulated_particles[ind.value()];
    sim_part.aux_force_scale = scl;
  } else {
    assert(false);
  }
}

void ProceduralFlowerOrnamentParticles::update(const UpdateInfo& update_info) {
  simulation_timer.on_frame_entry(update_info.real_dt);
  Stopwatch abort_guard;

  while (simulation_timer.should_proceed(sim_dt())) {
    simulate(simulated_particles, update_info);
    if (simulation_timer.on_after_simulate_check_abort(sim_dt(), abort_guard, sim_dt() * 0.1)) {
      GROVE_LOG_WARNING_CAPTURE_META("Simulation aborted early.", logging_id());
      break;
    }
  }

  const auto time_alpha = float(simulation_timer.get_accumulated_time() / sim_dt());
  for (int i = 0; i < int(particles.size()); i++) {
    const auto& sim_particle = simulated_particles[i];
    auto& sim_last = sim_particle.last;
    auto& sim_curr = sim_particle.curr;
    auto& particle = particles[i];

    particle.position = lerp(time_alpha, sim_last.position, sim_curr.position);
    particle.velocity = lerp(time_alpha, sim_last.velocity, sim_curr.velocity);
  }
}

GROVE_NAMESPACE_END
