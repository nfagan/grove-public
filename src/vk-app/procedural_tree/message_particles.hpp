#pragma once

#include "tree_message_system.hpp"

namespace grove::tree {

struct MessageParticles {
public:
  struct Particle {
    msg::MessageID associated_message;
    Vec3f canonical_offset;
    Vec3f current_offset;
    Vec3f message_position;
    Vec3f position;
    Vec2f rotation;
    uint32_t color;
    float rot_osc_phase;
    float osc_phase;
    float scale_osc_phase;
    float osc_freq;
    float lerp_speed;
    float center_scale;
    float current_scale;
    bool target_expand;
    float current_additional_scale;
  };

  using ViewMessages = ArrayView<const TreeMessageSystem::Message>;

public:
  ArrayView<const Particle> update(const ViewMessages& messages, double dt);
  void push_particle(const Particle& particle);
  static Particle make_default_particle(msg::MessageID msg, const Vec3f& at_pos);

public:
  std::vector<Particle> particles;
  int num_particles{};
  bool particles_modified{};
};

}