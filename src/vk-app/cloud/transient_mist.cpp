#include "transient_mist.hpp"
#include "distribute_points.hpp"
#include "../terrain/terrain.hpp"
#include "grove/math/random.hpp"
#include "grove/math/util.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Temporary.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace fog;

float eval_opacity(const Vec3f& position, const Vec3f& camera_position,
                   float elapsed_time, float total_time, float dist_begin_atten) {
  float cam_dist = (position - camera_position).length();
  float dist_scale = 1.0f;
  if (cam_dist < dist_begin_atten) {
    dist_scale = std::pow(cam_dist / dist_begin_atten, 2.0f);
  }

  float fade_scale = 1.0f;
  const float fade_beg = 0.2f;
  auto frac_elapsed = clamp(elapsed_time / total_time, 0.0f, 1.0f);
  if (frac_elapsed < fade_beg) {
    fade_scale = frac_elapsed / fade_beg;
  } else if (frac_elapsed >= 1.0f - fade_beg) {
    fade_scale = (1.0f - frac_elapsed) / fade_beg;
  }

  float opacity = 1.0f;
  return opacity * dist_scale * std::pow(fade_scale, 2.0f);
}

} //  anon

void fog::distribute_transient_mist_elements(TransientMistElement* elements, int num_elements) {
  constexpr int stack_size = 128;
  Temporary<Vec2f, stack_size> store_dst_ps;
  Temporary<bool, stack_size> store_accept_ps;

  auto* dst_ps = store_dst_ps.require(num_elements);
  auto* accept_ps = store_accept_ps.require(num_elements);
  float r = points::place_outside_radius_default_radius(num_elements, 0.9f);

  points::place_outside_radius<Vec2f, float, 2>(dst_ps, accept_ps, num_elements, r);
  for (int i = 0; i < num_elements; i++) {
    auto p11 = dst_ps[i] * 2.0f - 1.0f;
    elements[i].normalized_translation = Vec3f{p11.x, 0.0f, p11.y};
  }
}

void fog::tick_transient_mist(TransientMistElement* elements, int num_elements,
                              const TransientMistTickParams& params) {
  bool all_elapsed{true};
  for (int i = 0; i < num_elements; i++) {
    auto& el = elements[i];
    el.elapsed_time += params.real_dt;
    if (!el.elapsed) {
      if (el.elapsed_time >= el.total_time) {
        el.elapsed_time = el.total_time;
        el.elapsed = true;
      } else {
        all_elapsed = false;
      }
      el.opacity = eval_opacity(
        el.position,
        *params.camera_position,
        el.elapsed_time,
        el.total_time,
        params.dist_begin_attenuation);
    }
  }

  if (all_elapsed) {
    auto cam_fxz = normalize(Vec3f{params.camera_forward->x, 0.0f, params.camera_forward->z});
    auto cam_rxz = normalize(Vec3f{params.camera_right->x, 0.0f, params.camera_right->z});
    auto& dist_front = params.camera_front_distance_limits;
    auto& dist_right = params.camera_right_distance_limits;

    float cam_dist = lerp(urandf(), dist_front.x, dist_front.y);
    float cam_span = lerp(urandf(), dist_right.x, dist_right.y);
    auto cluster_ori = *params.camera_position + cam_fxz * cam_dist + cam_rxz * cam_span;
    const float trans_span = params.grid_size;

    for (int i = 0; i < num_elements; i++) {
      auto& el = elements[i];
      el.elapsed_time = 0.0f;
      el.elapsed = false;
      el.position = el.normalized_translation * Vec3f{trans_span, 0.0f, trans_span} + cluster_ori;
      el.position.y = params.terrain->height_nearest_position_xz(el.position) + params.y_offset;
    }
  }
}

GROVE_NAMESPACE_END
