#pragma once

#include "grove/math/vector.hpp"
#include "grove/math/Bounds3.hpp"
#include "grove/common/SimulationTimer.hpp"
#include "grove/common/identifier.hpp"
#include "grove/common/ArrayView.hpp"
#include "grove/common/DynamicArray.hpp"
#include <vector>

namespace grove {

struct RainParticleGroupID {
  GROVE_INTEGER_IDENTIFIER_EQUALITY(RainParticleGroupID, id)
  GROVE_INTEGER_IDENTIFIER_INEQUALITIES(RainParticleGroupID, id)

  uint32_t id;
};

class SpatiallyVaryingWind;
class Camera;

class RainParticles {
public:
  struct ParticleSimulationState {
    Vec3f position;
    Vec3f velocity;
    Vec3f force;
    float alpha;
  };

  struct Particle {
    Vec3f position;
    float alpha;
    Vec3f velocity;
    float rand01;
    Vec3f expired_position;
    bool expired;
  };

  struct SimulatedParticle {
    float mass;
    float alpha_incr;
    ParticleSimulationState last;
    ParticleSimulationState curr;
  };

  struct SimulatedGroup {
    RainParticleGroupID id{};
    std::vector<SimulatedParticle> simulated_particles;
    std::vector<Particle> particles;
    Bounds3f extent;
  };

  struct GroupParams {
    Vec3f origin{};
    Bounds3f extent{};
    int num_particles{};
  };

  struct UpdateInfo {
    const SpatiallyVaryingWind& wind;
    const Vec3f& origin;
    double real_dt;
    double dt_scale;
  };

  struct ExpiredParticle {
    RainParticleGroupID group_id;
    Vec3f position;
  };

  using ExpiredParticles = DynamicArray<ExpiredParticle, 4>;

  struct UpdateResult {
    ArrayView<ExpiredParticle> expired_particles;
  };

public:
  RainParticleGroupID push_group(const GroupParams& params);
  UpdateResult update(const UpdateInfo& update_info);
  const SimulatedGroup* get_group(RainParticleGroupID id) const;

private:
  std::vector<SimulatedGroup> groups;
  ExpiredParticles expired_particles;
  SimulationTimer simulation_timer;

  uint32_t next_group_id{1};
};

}