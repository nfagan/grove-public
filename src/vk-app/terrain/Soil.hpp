#pragma once

#include "grove/math/vector.hpp"

namespace grove {

struct SoilImpl;

namespace gen {
struct SlimeMoldConfig;
}

class Soil {
public:
  Soil();
  ~Soil();
  Soil(Soil&& other) noexcept;
  Soil& operator=(Soil&& other) noexcept;
  Soil(const Soil& other) = delete;
  Soil& operator=(const Soil& other) = delete;

  void initialize();
  void update();
  Vec3f sample_quality01(const Vec2f& world_position_xz, float radius_world) const;
  void add_quality01(const Vec2f& world_position_xz, float radius_world, const Vec3f& value);
  float to_length01(float v) const;
  Vec2f to_position01(const Vec2f& p_xz) const;

  void set_particle_turn_speed_power(int pow);
  void set_particle_speed_power(int pow);
  void set_particle_use_only_right_turns(bool v);
  const float* read_image_data() const;
  const gen::SlimeMoldConfig* read_config() const;
  gen::SlimeMoldConfig* get_config();

private:
  SoilImpl* impl{};
};

}