#include "petal.hpp"
#include "grove/common/common.hpp"
#include "grove/math/random.hpp"

GROVE_NAMESPACE_BEGIN

using namespace petal;

namespace {

int maybe_random_perm_index(int ind) {
  return ind < 0 ? MaterialParams::random_perm_index() : ind;
}

} //  anon

ShapeParams ShapeParams::lilly(float growth_frac, float radius_scale) {
  petal::ShapeParams result{};
  result.min_radius = 0.05f;
  result.radius = 2.0f * radius_scale * growth_frac;
  result.radius_power = 2.0f;
  result.max_additional_radius = 0.1f;
  result.circumference_frac0 = 1.0f;
  result.circumference_frac1 = 0.05f;
  result.circumference_frac_power = 0.5f;
  result.curl_scale = -0.25f * radius_scale * growth_frac;
  result.scale = Vec2f{0.75f, 1.0f};
  result.group_frac = 0.0f;
  result.max_negative_y_offset = 0.1f;
  return result;
}

ShapeParams ShapeParams::alla(float growth_frac, float radius_scale) {
  petal::ShapeParams result{};
  result.min_radius = 0.05f;
  result.radius = 0.1f * growth_frac * radius_scale;
  result.radius_power = 1.0f;
  result.max_additional_radius = 0.1f;
  result.circumference_frac0 = 1.0f;
  result.circumference_frac1 = 1.0f;
  result.circumference_frac_power = 0.5f;
  result.curl_scale = 0.0f;
  result.scale = Vec2f{1.0f, 2.0f};
  result.group_frac = 0.0f;
  result.max_negative_y_offset = 0.1f;
  return result;
}

ShapeParams ShapeParams::tulip(float growth_frac, float radius_scale) {
  petal::ShapeParams result{};
  result.min_radius = 0.05f;
  result.radius = 0.1f * growth_frac * radius_scale;
  result.radius_power = 0.5f;
  result.max_additional_radius = 0.1f;
  result.circumference_frac0 = 0.75f;
  result.circumference_frac1 = 0.25f;
  result.circumference_frac_power = 2.0f;
  result.curl_scale = 0.0f;
  result.scale = Vec2f{1.0f, 1.0f};
  result.group_frac = 0.0f;
  result.max_negative_y_offset = 0.2f;
  return result;
}

ShapeParams ShapeParams::plane(float growth_frac, float death_frac,
                               float radius_scale, float radius_power) {
  petal::ShapeParams result{};
  result.min_radius = 0.05f;
  result.radius = growth_frac * radius_scale;
  result.radius_power = radius_power;
  result.mix_texture_color = 0.0f;
  result.circumference_frac0 = 1.0f;
  result.circumference_frac1 = 1.0f;
  result.circumference_frac_power = 2.0f;
  result.curl_scale = -death_frac * radius_scale;
  result.scale = Vec2f{1.0f, 1.0f};
  result.group_frac = 0.0f;
  result.min_z_discard_enabled = 0.75f;
  return result;
}

int MaterialParams::random_perm_index() {
  return int(urand() * 6.0);
}

MaterialParams MaterialParams::type0(int pi) {
  MaterialParams result{};
  result.color_info0 = Vec4f{1.0f, 1.0f, 0.0f, 1.0f};
  result.color_component_indices = component_indices_from_perm_index(maybe_random_perm_index(pi));
  return result;
}

MaterialParams MaterialParams::type1(int pi) {
  MaterialParams result{};
  result.color_info0 = Vec4f{0.25f, 0.25f, 0.5f, 2.0f};
  result.color_component_indices = component_indices_from_perm_index(maybe_random_perm_index(pi));
  return result;
}

MaterialParams MaterialParams::type2(int pi) {
  MaterialParams result{};
  result.color_info0 = Vec4f{1.0f, 0.5f, 1.0f, 0.5f};
  result.color_component_indices = component_indices_from_perm_index(maybe_random_perm_index(pi));
  return result;
}

MaterialParams MaterialParams::type3(int pi) {
  MaterialParams result{};
  result.color_info0 = Vec4f{0.1f, 0.1f, 0.05f, 1.0f};
  result.color_component_indices = component_indices_from_perm_index(maybe_random_perm_index(pi));
  return result;
}

Vec3<int> MaterialParams::component_indices_from_perm_index(int pi) {
  switch (pi) {
    case 0:
      return {0, 1, 2};
    case 1:
      return {1, 0, 2};
    case 2:
      return {0, 2, 1};
    case 3:
      return {1, 2, 0};
    case 4:
      return {2, 1, 0};
    case 5:
      return {2, 0, 1};
    default:
      return {0, 1, 2};
  }
}

GROVE_NAMESPACE_END
