#pragma once

#include "grove/common/Stopwatch.hpp"
#include "grove/math/vector.hpp"
#include "grove/audio/oscillator.hpp"
#include <vector>

namespace grove {

class WindParticles {
public:
  struct ParticleInstanceData {
    Vec3f position;
    Vec3f rotation_scale;
  };

  struct ParticleMetaData {
    float alpha_increment;
    float rot_y_increment;
    Vec3f initial_position;
    osc::Sin lfo0;
  };

public:
  void initialize(int num_particles);
  void update(const Vec3f& player_pos, const Vec2f& wind_vel);

  const std::vector<ParticleInstanceData>& read_instance_data() const {
    return instance_data;
  }

private:
  std::vector<ParticleInstanceData> instance_data;
  std::vector<ParticleMetaData> meta_data;
  Stopwatch stopwatch;
};

}