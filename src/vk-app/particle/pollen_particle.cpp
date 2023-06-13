#include "pollen_particle.hpp"
#include "../wind/SpatiallyVaryingWind.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include "grove/math/constants.hpp"
#include "grove/math/random.hpp"
#include "grove/math/util.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

Vec3f initial_force() {
  return Vec3f{
    urand_11f() * 1000.0f,
    1000.0f + urand_11f() * 200.0f,
    urand_11f() * 1000.0f
  };
}

float particle_mass() {
  return 1.0f + urand_11f() * 0.2f;
}

} //  anon

PollenParticle PollenParticles::create_particle(const Vec3f& p) {
  auto id = PollenParticleID{next_particle_id++};
  PollenParticle particle{id, p, urandf()};
  particles.push_back(particle);

  ParticleSimulationState state0{p, Vec3f{}, initial_force()};
  simulated_particles.push_back(SimulatedParticle{particle_mass(), state0, state0});
  return particle;
}

void PollenParticles::remove_particle(PollenParticleID id) {
  auto it = std::find_if(particles.begin(), particles.end(), [id](const auto& p) {
    return p.id == id;
  });

  if (it != particles.end()) {
    auto idx = it - particles.begin();
    particles.erase(it);
    simulated_particles.erase(simulated_particles.begin() + idx);
  } else {
    assert(false);
  }
}

ArrayView<const PollenParticle> PollenParticles::read_particles() const {
  return make_data_array_view<const PollenParticle>(particles);
}

void PollenParticles::simulate(const SpatiallyVaryingWind& wind, double sim_dt) {
  const auto sim_dt2 = sim_dt * sim_dt;

  for (auto& particle : simulated_particles) {
    particle.last = particle.curr;

    auto& state = particle.curr;
    const auto f_wind_xz = wind.wind_force(Vec2f{state.position.x, state.position.z});
    const auto f_wind = Vec3f{f_wind_xz.x, 0.0f, f_wind_xz.y};
    const auto f_g = Vec3f{0.0f, -9.8f, 0.0f};
//    const auto f = (f_wind * 1000.0f + f_g * 10.0f) + state.force;
    const auto f = (f_wind * 1000.0f + f_g * 30.0f) + state.force;

    const auto m = particle.mass;
    auto p = state.position + state.velocity * float(sim_dt) + 0.5f * f / m * float(sim_dt2);

    state.velocity = p - state.position;
    state.position = p;

    for (int i = 0; i < 3; i++) {
      auto pf = state.force[i];
      const auto scl = 256.0f;

      if (std::signbit(pf)) {
        pf += float(scl * sim_dt);
        if (pf > 0.0f) {
          pf = 0.0f;
        }
      } else {
        pf -= float(scl * sim_dt);
        if (pf < 0.0f) {
          pf = 0.0f;
        }
      }

      state.force[i] = pf;
    }
  }
}

PollenParticles::UpdateResult PollenParticles::update(const SpatiallyVaryingWind& wind,
                                                      double real_dt) {
  UpdateResult result;
  assert(simulated_particles.size() == particles.size());

  const auto sim_dt = 1.0 / 60.0;
  Stopwatch abort_guard;
  simulation_timer.on_frame_entry(real_dt);

  while (simulation_timer.should_proceed(sim_dt)) {
    simulate(wind, sim_dt);

    if (simulation_timer.on_after_simulate_check_abort(sim_dt, abort_guard, sim_dt * 0.5)) {
      GROVE_LOG_WARNING_CAPTURE_META("Simulation aborted early.", "PollenParticles");
      break;
    }
  }

  const auto time_alpha = simulation_timer.get_accumulated_time() / sim_dt;

  for (int i = 0; i < int(particles.size()); i++) {
    auto& sim_particle = simulated_particles[i];
    auto& particle = particles[i];

    particle.position =
      lerp(float(time_alpha), sim_particle.last.position, sim_particle.curr.position);

    if (particle.position.y < 2.0f) {
      ParticleEndOfLife to_terminate{particle.id, particle.position};
      result.to_terminate.push_back(to_terminate);
    }
  }

  return result;
}

GROVE_NAMESPACE_END
