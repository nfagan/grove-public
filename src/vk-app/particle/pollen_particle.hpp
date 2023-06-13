#pragma once

#include "grove/math/vector.hpp"
#include "grove/common/Stopwatch.hpp"
#include "grove/common/ArrayView.hpp"
#include "grove/common/DynamicArray.hpp"
#include "grove/common/SimulationTimer.hpp"
#include "grove/common/identifier.hpp"
#include <vector>
#include <functional>

namespace grove {

class SpatiallyVaryingWind;

struct PollenParticleID {
  GROVE_INTEGER_IDENTIFIER_STD_HASH(Hash, PollenParticleID, id)
  GROVE_INTEGER_IDENTIFIER_EQUALITY(PollenParticleID, id)
  GROVE_INTEGER_IDENTIFIER_INEQUALITIES(PollenParticleID, id)

  uint64_t id;
};

struct PollenParticle {
  PollenParticleID id;
  Vec3f position;
  float rand01;
};

class PollenParticles {
private:
  struct ParticleSimulationState {
    Vec3f position;
    Vec3f velocity;
    Vec3f force;
  };

  struct SimulatedParticle {
    float mass;
    ParticleSimulationState last;
    ParticleSimulationState curr;
  };

public:
  struct ParticleEndOfLife {
    PollenParticleID id{};
    Vec3f terminal_position{};
  };

  struct UpdateResult {
    DynamicArray<ParticleEndOfLife, 2> to_terminate;
  };

public:
  PollenParticle create_particle(const Vec3f& p0);
  void remove_particle(PollenParticleID id);
  ArrayView<const PollenParticle> read_particles() const;

  UpdateResult update(const SpatiallyVaryingWind& wind, double real_dt);

  std::size_t num_particles() const {
    return particles.size();
  }

private:
  void simulate(const SpatiallyVaryingWind& wind, double sim_dt);

private:
  std::vector<PollenParticle> particles;
  std::vector<SimulatedParticle> simulated_particles;
  uint64_t next_particle_id{1};

  SimulationTimer simulation_timer;
};

}