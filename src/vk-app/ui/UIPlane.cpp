#include "UIPlane.hpp"
#include "grove/common/common.hpp"
#include "grove/math/Ray.hpp"
#include "grove/math/intersect.hpp"

GROVE_NAMESPACE_BEGIN

void UIPlane::update(const Ray& mouse_ray, const Vec4f& plane, const Bounds3f& world_bound) {
  mouse_hit_info.hit = false;

  float hit_t;
  bool intersects = ray_plane_intersect(mouse_ray, plane, &hit_t);
  if (!intersects || hit_t < 0.0f) {
    return;
  }

  auto point = mouse_ray(hit_t);

  constexpr int z_dim = 1;
  const Vec2f p0{world_bound.min.x, world_bound.min[z_dim]};
  const Vec2f p1{world_bound.max.x, world_bound.max[z_dim]};

  if (point.x < p0.x || point[z_dim] < p0.y ||
      point.x >= p1.x || point[z_dim] >= p1.y) {
    return;
  }

  auto frac_xz = (Vec2f(point.x, point[z_dim]) - p0) / (p1 - p0);
  frac_xz.x = 1.0f - frac_xz.x;

  mouse_hit_info.frac_hit_point = frac_xz;
  mouse_hit_info.hit = true;
}

GROVE_NAMESPACE_END
