#pragma once

#include "grove/common/identifier.hpp"
#include "grove/common/SimulationTimer.hpp"
#include "grove/math/vector.hpp"
#include "grove/common/Optional.hpp"
#include <vector>
#include <cstdint>

namespace grove {

class SpatiallyVaryingWind;

class ProceduralFlowerOrnamentParticles {
public:
  struct Handle {
    GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
    GROVE_INTEGER_IDENTIFIER_EQUALITY(Handle, id)
    uint32_t id;
  };

  struct ParticleSimulationState {
    Vec3f position;
    Vec3f velocity;
    Vec3f force;
  };

  struct Particle {
    Handle handle;
    Vec3f origin;
    Vec3f position;
    Vec3f velocity;
  };

  struct SimulatedParticle {
    float mass;
    float force_decay_scale;
    float wind_force_scale;
    float aux_force_scale;
    Vec3f aux_force_direction;
    ParticleSimulationState last;
    ParticleSimulationState curr;
  };

  struct SpawnParams {
    Vec3f origin{};
    float initial_force_scale{1.0f};
    float wind_force_scale{1.0f};
  };

  struct UpdateInfo {
    const SpatiallyVaryingWind& wind;
    double real_dt;
    double dt_scale;
  };

public:
  void initialize();
  void update(const UpdateInfo& update_info);
  Handle spawn_particle(const SpawnParams& params);
  void remove_particle(Handle handle);
  Vec3f get_displacement(Handle handle);
  void set_auxiliary_force_scale(Handle handle, float scl);
  int num_particles() const {
    return int(particles.size());
  }

private:
  Optional<int> find_particle_index(Handle handle) const;

private:
  uint32_t next_particle_id{1};

  SimulationTimer simulation_timer;
  std::vector<Particle> particles;
  std::vector<SimulatedParticle> simulated_particles;
};

}