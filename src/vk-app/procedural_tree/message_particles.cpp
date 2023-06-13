#include "message_particles.hpp"
#include "grove/audio/oscillator.hpp"
#include "grove/common/common.hpp"
#include "grove/common/pack.hpp"
#include "grove/math/util.hpp"
#include "grove/math/random.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace tree;

using Particle = MessageParticles::Particle;
using ViewMessages = MessageParticles::ViewMessages;
using ViewParticles = ArrayView<const Particle>;

bool less_by_id(const Particle& a, const Particle& b) {
  return a.associated_message < b.associated_message;
}

msg::MessageID invalid_id() {
  return msg::MessageID{~uint64_t(0)};
}

const TreeMessageSystem::Message* find_message(const ViewMessages& messages, msg::MessageID id) {
  for (auto& msg : messages) {
    if (msg.message.id == id) {
      return &msg;
    }
  }
  return nullptr;
}

uint32_t to_u32_color(const Vec3f& color) {
  auto col = clamp_each(color, Vec3f{}, Vec3f{1.0f}) * 255.0f;
  return pack::pack_4u8_1u32(uint8_t(col.x), uint8_t(col.y), uint8_t(col.z), uint8_t(255));
}

int partition_expired(std::vector<Particle>& particles,
                      const ViewMessages& messages, int num_particles) {
  int beg{};
  int new_num_particles{};
  int any_erased{};

  while (beg < num_particles) {
    auto id = particles[beg].associated_message;
    auto* msg_it = find_message(messages, id);
    const bool has_message = msg_it != nullptr;
    const Vec3f message_pos = msg_it ? msg_it->position : Vec3f{};
    const Vec3f message_color = msg_it ? msg_it->message.data.read_vec3f() : Vec3f{};
    const auto assign_id = has_message ? id : invalid_id();
    const uint32_t color = to_u32_color(message_color);
    const bool target_expand = msg_it && msg_it->events.just_reached_new_leaf;

    int end = beg;
    while (end < num_particles && particles[end].associated_message == id) {
      auto& part = particles[end++];
      part.associated_message = assign_id;
      part.message_position = message_pos;
      part.color = color;
      part.target_expand |= target_expand;
    }

    if (has_message) {
      new_num_particles += (end - beg);
    }

    any_erased |= int(!has_message);
    beg = end;
  }

  if (any_erased) {
    std::sort(particles.begin(), particles.end(), less_by_id);
  }

  return new_num_particles;
}

} //  anon

ViewParticles tree::MessageParticles::update(const ViewMessages& messages, double dt) {
  if (particles_modified) {
    std::sort(particles.begin(), particles.begin() + num_particles, less_by_id);
    particles_modified = false;
  }
  num_particles = partition_expired(particles, messages, num_particles);

  const double update_rate = 1.0 / clamp(dt, 1.0/1e3, 1.0/15.0);
  const float max_addtl_scale = 0.125f;

  for (int i = 0; i < num_particles; i++) {
    auto& part = particles[i];
    const auto t = float(1.0 - std::pow(double(part.lerp_speed), dt));
    auto osc_dir = normalize(part.canonical_offset);

    double phs = part.osc_phase;
    auto osc_val = float(osc::Sin::tick(update_rate, &phs, part.osc_freq));

    double rot_phs = part.rot_osc_phase;
    auto rot_osc_val = float(osc::Sin::tick(update_rate, &rot_phs, part.osc_freq * 0.1f));

    double scale_phs = part.scale_osc_phase;
    auto scale_osc_val = float(osc::Sin::tick(update_rate, &scale_phs, part.osc_freq * 0.25f));

    part.osc_phase = float(phs);
    part.rot_osc_phase = float(rot_phs);
    part.scale_osc_phase = float(scale_phs);

    float targ_addtl_scale = part.target_expand ? max_addtl_scale : 0.0f;
    part.current_additional_scale = lerp(t, part.current_additional_scale, targ_addtl_scale);
    part.target_expand &= !(std::abs(part.current_additional_scale - targ_addtl_scale) < 1e-3f);

    const float center_scale = part.center_scale + part.current_additional_scale;
    part.current_offset = part.canonical_offset + osc_dir * osc_val * 0.5f;
    part.rotation.y = rot_osc_val * pif();
    part.current_scale = center_scale + scale_osc_val * center_scale * 0.25f;
    part.position = lerp(t, part.position, part.message_position);
  }

  return {particles.data(), particles.data() + num_particles};
}

void tree::MessageParticles::push_particle(const Particle& particle) {
  if (num_particles == int(particles.size())) {
    particles.resize(num_particles == 0 ? 16 : num_particles * 2);
  }
  particles[num_particles++] = particle;
  particles_modified = true;
}

Particle tree::MessageParticles::make_default_particle(msg::MessageID msg, const Vec3f& at_pos) {
  Particle part{};
  part.associated_message = msg;
  part.canonical_offset = Vec3f{urand_11f(), urand_11f(), urand_11f()};
  part.current_offset = part.canonical_offset;
  part.osc_freq = lerp(urandf(), 0.75f, 1.5f);
  part.osc_phase = urandf() * pif();
  part.rot_osc_phase = urandf() * pif();
  part.scale_osc_phase = urandf() * pif();
  part.position = at_pos;
  part.rotation.x = urandf() * 2.0f * pif();
  part.rotation.y = urandf() * 2.0f * pif();
  part.lerp_speed = lerp(urandf(), 0.00125f * 0.25f, 0.00125f * 0.5f);
  part.center_scale = lerp(urandf(), 0.25f * 0.125f, 0.5f * 0.125f);
  part.current_scale = part.center_scale;
  return part;
}

GROVE_NAMESPACE_END
