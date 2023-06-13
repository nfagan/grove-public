#include "OrbitCamera.hpp"
#include "grove/common/common.hpp"
#include "grove/math/matrix_transform.hpp"
#include "grove/math/string_cast.hpp"
#include "grove/math/util.hpp"
#include <iostream>

GROVE_NAMESPACE_BEGIN

OrbitCamera::OrbitCamera() :
  target(0.0f),
  front(0.0f, 0.0f, -1.0f),
  follow_distance(20.0f),
  view(1.0f),
  projection(1.0f) {
  //
}

void OrbitCamera::move(const Vec3f& deltas) {
  target += deltas;
}

void OrbitCamera::rotate(const Vec3f& deltas) {
  const float theta_eps = 0.01f;
  const float min_theta = theta_eps;
  const float max_theta = grove::pif() - theta_eps;

  front = spherical_rotate(front, deltas.x, deltas.y, min_theta, max_theta);
}

void OrbitCamera::set_projection_info(const ProjectionInfo& info) {
  projection_info = info;
}

void OrbitCamera::set_position(const Vec3f& pos) {
  const auto delta = pos - get_position();
  target += delta;
}

void OrbitCamera::set_target(const Vec3f& targ) {
  target = targ;
}

void OrbitCamera::set_follow_distance(float dist) {
  follow_distance = dist;
}

float OrbitCamera::get_follow_distance() const {
  return follow_distance;
}

Vec3f OrbitCamera::get_front() const {
  return front;
}

Vec3f OrbitCamera::get_front_xz() const {
  auto f = get_front();
  f.y = 0.0f;
  return normalize(f);
}

Vec3f OrbitCamera::get_right() const {
  return Vec3f(view(0, 0), view(0, 1), view(0, 2));
}

Vec3f OrbitCamera::get_position() const {
  return target + -front * follow_distance;
}

Mat4f OrbitCamera::get_projection() const {
  return projection;
}

Mat4f OrbitCamera::get_view() const {
  return view;
}

ProjectionInfo OrbitCamera::get_projection_info() const {
  return projection_info;
}

const Vec3f& OrbitCamera::get_target() const {
  return target;
}

void OrbitCamera::update_view() {
  view = look_at(get_position(), target, Vec3f(0.0f, 1.0f, 0.0f));
}

void OrbitCamera::update_projection() {
  projection = projection_from_info(projection_info);
}

GROVE_NAMESPACE_END
