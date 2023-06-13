#pragma once

#include "pollen_particle.hpp"

namespace grove {

class PollenParticleRenderer;

class PollenComponent {
public:
  struct UpdateInfo {
    const SpatiallyVaryingWind& wind;
    double real_dt;
    PollenParticleRenderer& particle_renderer;
  };
  struct UpdateResult {
    PollenParticles::UpdateResult particle_update_res;
  };
public:
  void initialize();
  UpdateResult update(const UpdateInfo& info);
public:
  PollenParticles pollen_particles;
  std::vector<PollenParticleID> debug_particles;
};

}