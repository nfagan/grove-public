#include "environment_input.hpp"
#include "grove/common/common.hpp"
#include "grove/common/profile.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

inline bool in_sphere(const Vec3f& sc, float r, const Vec3f& p) {
  auto to_point = p - sc;
  return to_point.length_squared() <= r * r;
}

} //  anon

void tree::consume_within_occupancy_zone(TreeID id, const Bud& bud, AttractionPoints& points) {
  auto radius = bud.occupancy_zone_radius;
  points.map_over_sphere([&bud, radius, id](auto* node) {
    if (node->data.is_active() && in_sphere(bud.position, radius, node->data.position)) {
      if (!node->data.is_consumed()) {
        node->data.set_id(id.id);
        node->data.set_consumed(true);
      }
    }
  }, bud.position, radius);
}

tree::EnvironmentInputs tree::compute_environment_input(const ClosestPointsToBuds& closest) {
  auto profiler = GROVE_PROFILE_SCOPE_TIC_TOC("tree/compute_environment_input");

  tree::EnvironmentInputs result;

  for (auto& [node, bud] : closest) {
    assert(!node->data.is_consumed() && node->data.is_active());
    auto bud_d = normalize(node->data.position - bud.position);

    auto maybe_exists = result.find(bud.id);
    if (maybe_exists != result.end()) {
      maybe_exists->second.direction += bud_d;
      maybe_exists->second.q += 1.0f; //  use q temporarily as a counter.
    } else {
      tree::EnvironmentInput input{};
      input.direction = bud_d;
      input.q = 1.0f;
      result[bud.id] = input;
    }
  }

  for (auto& [_, res] : result) {
    assert(res.q >= 1.0f);
    res.direction /= res.q;
    res.q = 1.0f;
    res.direction = normalize_or_default(res.direction, Vec3f{0.0f, 1.0f, 0.0f});
  }

  return result;
}

void tree::sense_bud(const Bud& bud, AttractionPoints& points, SenseContext& context) {
  points.map_over_sphere([&](auto* node) {
    auto& nd = node->data;
    if (!nd.is_active() || nd.is_consumed()) {
      return;
    }

    auto to_point = nd.position - bud.position;
    auto to_point_d = to_point.length();

    if (to_point_d > 0.0f && to_point_d <= bud.perception_distance) {
      //  Within perception distance.
      auto to_point_v = to_point / to_point_d;
      auto v_sim = dot(to_point_v, bud.direction);
      auto a_sim = std::acos(std::abs(v_sim));
      if (a_sim < bud.perception_angle * 0.5f) {
        //  Within perception volume.
        auto existing_it = context.closest_points_to_buds.find(node);
        if (existing_it == context.closest_points_to_buds.end()) {
          context.closest_points_to_buds[node] = bud;
        } else {
          //  Check whether this bud is closer to the attraction point than the existing bud.
          auto& existing_bud = existing_it->second;
          auto existing_dist = (nd.position - existing_bud.position).length();
          if (to_point_d < existing_dist) {
            //  This bud is closer to the point.
            context.closest_points_to_buds[node] = bud;
          }
        }
      }
    }
  }, bud.position, bud.perception_distance);
}

GROVE_NAMESPACE_END
