#include "Camera.hpp"
#include "grove/common/common.hpp"
#include "grove/math/constants.hpp"
#include "grove/math/util.hpp"
#include "grove/math/matrix_transform.hpp"
#include "grove/math/frame.hpp"
#include "grove/math/Frustum.hpp"
#include "Window.hpp"

GROVE_NAMESPACE_BEGIN

ProjectionInfo::ProjectionInfo() :
  near(0.1f),
  far(std::numeric_limits<float>::infinity()),
  aspect_ratio(1.0f),
  fov_y(float(grove::pi_over_four())) {
  //
}

float ProjectionInfo::projection_plane_distance() const {
  return 1.0f / std::tan(fov_y * 0.5f);
}

Frustum Camera::make_world_space_frustum(float far) const {
  const auto inv_view = inverse(get_view());
  const auto proj_info = get_projection_info();
  const float s = proj_info.aspect_ratio;
  const float g = proj_info.projection_plane_distance();
  const float n = proj_info.near;
  const float f = far == 0.0f ? proj_info.far : far;

  auto v0 = to_vec3(inv_view[0]);
  auto v1 = to_vec3(inv_view[1]);
  auto v2 = to_vec3(inv_view[2]);
  return grove::make_world_space_frustum(s, g, n, f, v0, v1, v2, get_position());
}

Vec3f Camera::spherical_rotate(const Vec3f& front, float dtheta, float dphi,
                               float theta_min, float theta_max) {
  auto curr_spherical = cartesian_to_spherical(front);
  curr_spherical.x = clamp(curr_spherical.x + dtheta, theta_min, theta_max);
  curr_spherical.y += dphi;
  return spherical_to_cartesian(curr_spherical);
}

Mat4f Camera::projection_from_info(const ProjectionInfo& info) {
  const auto fov_y = info.fov_y;
  const auto ar = info.aspect_ratio;
  const auto near = info.near;

  return infinite_perspective_reverses_depth(fov_y, ar, near);
}

void Camera::set_default_projection_info(Camera& camera, float aspect_ratio) {
  auto info = camera.get_projection_info();

  info.aspect_ratio = aspect_ratio;
  info.fov_y = float(grove::pi_over_four());
  info.near = 0.1f;

  camera.set_projection_info(info);
}

Vec2f Camera::get_position_xz() const {
  auto pos = get_position();
  return Vec2f{pos.x, pos.z};
}

GROVE_NAMESPACE_END