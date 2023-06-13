#include "Soil.hpp"
#include "../generative/slime_mold.hpp"
#include "grove/common/common.hpp"
#include "grove/math/Bounds2.hpp"

GROVE_NAMESPACE_BEGIN

struct SoilImpl {
  gen::SlimeMoldParams params;
  gen::SlimeMoldConfig config;
  gen::SlimeMoldSimulationContext sim_context{};
  gen::DefaultSlimeMoldSimulationTextureData texture_data;
  std::unique_ptr<gen::SlimeParticle[]> particles;
  bool initialized{};
};

namespace {

void set_sim_context_ptrs(gen::SlimeMoldSimulationContext& context,
                          gen::DefaultSlimeMoldSimulationTextureData& tex_data,
                          const gen::SlimeMoldParams* params) {
  context.texture_data0 = tex_data.texture_data0.get();
  context.texture_data1 = tex_data.texture_data1.get();
  context.texture_data2 = tex_data.texture_data2.get();
  context.signal_data = tex_data.signal_data.get();
  context.perturb_data = tex_data.perturb_data.get();
  context.params = params;
}

constexpr float world_span() {
  return 512.0f;
}

constexpr Bounds2f world_bound() {
  return Bounds2f{Vec2f{-world_span() * 0.5f}, Vec2f{world_span() * 0.5f}};
}

Vec2f world_position_to_fraction(const Vec2f& p_world) {
  const Bounds2f bounds = world_bound();
  return bounds.to_fraction(p_world);
}

float world_length_to_fraction(float v_world) {
  return v_world / world_span();
}

} //  anon

Soil::Soil(Soil&& other) noexcept : impl{other.impl} {
  other.impl = nullptr;
}

Soil& Soil::operator=(Soil&& other) noexcept {
  Soil tmp{std::move(other)};
  std::swap(impl, tmp.impl);
  return *this;
}

Soil::Soil() : impl{new SoilImpl()} {
  //
}

Soil::~Soil() {
  delete impl;
}

void Soil::initialize() {
  impl->texture_data = gen::make_default_slime_mold_texture_data();
  impl->particles = gen::make_slime_mold_particles(impl->config);
  set_sim_context_ptrs(impl->sim_context, impl->texture_data, &impl->params);
  impl->initialized = true;
}

void Soil::update() {
  if (impl->initialized) {
    gen::update_slime_mold_particles(
      impl->particles.get(),
      impl->config,
      &impl->sim_context);
  }
}

Vec3f Soil::sample_quality01(const Vec2f& world_position_xz, float radius_world) const {
  if (!impl->initialized) {
    return Vec3f{};
  }
  auto p01 = world_position_to_fraction(world_position_xz);
  float r01 = world_length_to_fraction(radius_world);
  return gen::sample_slime_mold_texture_data01(impl->texture_data.texture_data0.get(), p01, r01);
}

void Soil::add_quality01(const Vec2f& world_position_xz, float radius_world, const Vec3f& value) {
  if (impl->initialized) {
    gen::add_value(
      impl->texture_data.texture_data0.get(),
      world_position_to_fraction(world_position_xz),
      world_length_to_fraction(radius_world),
      value);
  }
}

float Soil::to_length01(float v) const {
  return world_length_to_fraction(v);
}

Vec2f Soil::to_position01(const Vec2f& p_xz) const {
  return world_position_to_fraction(p_xz);
}

void Soil::set_particle_turn_speed_power(int pow) {
  if (impl->initialized) {
    gen::set_particle_turn_speed_power(impl->particles.get(), impl->config, pow);
  }
}

void Soil::set_particle_speed_power(int pow) {
  if (impl->initialized) {
    gen::set_particle_speed_power(impl->particles.get(), impl->config, pow);
  }
}

void Soil::set_particle_use_only_right_turns(bool v) {
  if (impl->initialized) {
    gen::set_particle_right_only(impl->particles.get(), impl->config, v);
  }
}

const float* Soil::read_image_data() const {
  return impl->texture_data.texture_data0.get();
}

const gen::SlimeMoldConfig* Soil::read_config() const {
  return &impl->config;
}

gen::SlimeMoldConfig* Soil::get_config() {
  return &impl->config;
}

GROVE_NAMESPACE_END
