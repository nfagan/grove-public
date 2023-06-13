#pragma once

#include "grove/math/vector.hpp"
#include <memory>

namespace grove {

class UIPlaneCloth {
public:
  static constexpr int particle_dim = 40;
  static constexpr int num_particles = particle_dim * particle_dim;
  static constexpr float particle_mass = 1.0f;
  static constexpr float k_wind = 25.0f;
  static constexpr float rest_distance = 0.25f;
  static constexpr float rest_height = 2.5f;

  static constexpr float k_spring_adjacent = 512.0f;
  static constexpr float k_damp_adjacent = 256.0f;

  static constexpr float k_spring_far = 1024.0f;
  static constexpr float k_damp_far = 256.0f;

  static constexpr float k_spring_corner = 512.0f;
  static constexpr float k_damp_corner = 256.0f;

public:
  struct PositionData {
    const Vec4f* positions;
    int count;
    Vec3f bounds_p0;
    Vec3f bounds_p1;
    Vec4f plane;
  };

public:
  UIPlaneCloth();

  void update(float spectral_mean);
  void move(const Vec3f& vel);
  void set_spectral_multiplier(float value);

  PositionData get_position_data(float height) const;
  void on_new_height_map(float height);

private:
  void initialize_particles();
  void set_external_forces(float spectral_mean);
  void calculate_normals();
  Vec3f calculate_spring_force(float rest_dist,
                               float k_spring,
                               float k_damp,
                               int ind,
                               const Vec4f& p,
                               const Vec3f& v) const;

private:
  std::unique_ptr<Vec4f[]> positions; //  vec4 for texture access.
  std::unique_ptr<Vec3f[]> velocities;
  std::unique_ptr<Vec3f[]> normals;
  std::unique_ptr<Vec3f[]> spring_forces;
  std::unique_ptr<Vec3f[]> external_forces;

  Vec3f wind_direction;
  float spectral_mean_multiplier = 2.0f;
};

}