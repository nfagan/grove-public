#pragma once

#include "grove/math/vector.hpp"
#include <memory>

namespace grove::gen {

/*
 * Slime mold sim. References:
 *  1. https://sagejenson.com/physarum
 *  2. https://uwe-repository.worktribe.com/output/980579
 *  3. https://www.youtube.com/watch?v=X-iSQQgOd1A&t=923s
 */

struct SlimeMoldConfig {
  float dt() const {
    return 1.0f / 60.0f * time_scale;
  }

  static constexpr int texture_dim = 256;
  static constexpr int num_texture_channels = 3;
  int num_particles{1000};

  static constexpr float starting_offset_span = 0.1f;
  int filter_size{3};
  float decay{0.004f};
  float diffuse_speed{0.95f};
  bool diffuse_enabled{true};
  float time_scale{1.0f};

  int num_perturb_iters{1000};
  int perturb_interval{3000};
  int perturb_event_type{0};
  int num_perturb_circles{10};
  bool circular_world{true};
  bool allow_perturb_event{true};
  bool allow_signal_influence{true};

  int scale_speed_power{0};
  int turn_speed_power{0};
  bool only_right_turns{true};
};

struct SlimeMoldParams {
  float signal_value{};
  Vec2f signal_position{0.25f};
  float signal_radius{0.05f};
  Vec3f channel_mask{1.0f};
};

struct SlimeParticle {
  Vec2f position;
  float heading;
  float left_sensor;
  float right_sensor;
  float sensor_step_size;
  float sensor_size;
  float speed;
  float deposit;
  Vec3f channel_weights;
  float sensor_speed_sensitivity;
  float sensor_speed_sensitivity_scale;
  float turn_speed;
  bool right_only;
};

struct SlimeMoldSimulationContext {
  float* texture_data0;
  float* texture_data1;
  float* texture_data2;
  float* perturb_data;
  float* signal_data;
  bool set_perturb_data;
  int perturb_state;
  int perturb_iters;
  uint64_t tot_iter;
  const SlimeMoldParams* params;
};

struct DefaultSlimeMoldSimulationTextureData {
  std::unique_ptr<float[]> texture_data0;
  std::unique_ptr<float[]> texture_data1;
  std::unique_ptr<float[]> texture_data2;
  std::unique_ptr<float[]> perturb_data;
  std::unique_ptr<float[]> signal_data;
};

std::unique_ptr<float[]> make_slime_mold_texture_data();
DefaultSlimeMoldSimulationTextureData make_default_slime_mold_texture_data();
std::unique_ptr<SlimeParticle[]> make_slime_mold_particles(const SlimeMoldConfig& config);
void set_particle_turn_speed_power(SlimeParticle* particles,
                                   SlimeMoldConfig& config,
                                   int new_power);
void set_particle_speed_power(SlimeParticle* particles,
                              SlimeMoldConfig& config,
                              int new_power);
void set_particle_right_only(SlimeParticle* particles, SlimeMoldConfig& config, bool value);
Vec3f sample_slime_mold_texture_data01(const float* data, const Vec2f& p01, float r01);
void add_value(float* data, const Vec2f& p01, float radius01, const Vec3f& value);
void add_value(float* data,
               int tex_dim,
               int tex_components,
               const Vec2f& p01,
               float radius01,
               const Vec3f& value);
void update_slime_mold_particles(SlimeParticle* particles,
                                 const SlimeMoldConfig& config,
                                 SlimeMoldSimulationContext* context);

}