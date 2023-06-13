#include "PollenComponent.hpp"
#include "../render/PollenParticleRenderer.hpp"
#include "../render/render_particles_gpu.hpp"
#include "grove/common/common.hpp"
#include "grove/math/random.hpp"
#include "grove/math/util.hpp"

#define DEBUG_POLLEN (0)

GROVE_NAMESPACE_BEGIN

namespace {

[[maybe_unused]] Vec3f rand_position() {
  return Vec3f{urand_11f(), 0.0f, urand_11f()} * 8.0f + Vec3f{0.0f, 4.0f, 0.0f};
}

} //  anon

void PollenComponent::initialize() {
#if DEBUG_POLLEN
  for (int i = 0; i < 10; i++) {
    auto part = pollen_particles.create_particle(rand_position());
    debug_particles.push_back(part.id);
  }
#endif
}

PollenComponent::UpdateResult PollenComponent::update(const UpdateInfo& info) {
  PollenComponent::UpdateResult result;
  double real_dt = info.real_dt;
  real_dt = std::min(0.25, real_dt);
  result.particle_update_res = pollen_particles.update(info.wind, real_dt);

  for (auto& to_terminate : result.particle_update_res.to_terminate) {
    pollen_particles.remove_particle(to_terminate.id);
#if DEBUG_POLLEN
    //  Respawn debug particle
    auto debug_it = std::find(debug_particles.begin(), debug_particles.end(), to_terminate.id);
    if (debug_it != debug_particles.end()) {
      auto part = pollen_particles.create_particle(rand_position());
      *debug_it = part.id;
    }
#endif
  }

  for (auto& part : pollen_particles.read_particles()) {
#if 1
    const float s = 0.125f;
    particle::CircleQuadInstanceDescriptor quad_desc{};
    quad_desc.position = part.position;
    quad_desc.scale = s + (part.rand01 * 2.0f - 1.0f) * s * 0.25f;
    quad_desc.translucency = 0.5f;
    quad_desc.color = Vec3f{1.0f};
    particle::push_circle_quad_sample_depth_instances(&quad_desc, 1);
#else
    PollenParticleRenderer::DrawableParams params{};
    params.translation = part.position;
    params.scale = 0.1f;
    info.particle_renderer.push_drawable(params);
#endif
  }

  return result;
}

GROVE_NAMESPACE_END
