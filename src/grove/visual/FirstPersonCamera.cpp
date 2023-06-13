#include "FirstPersonCamera.hpp"
#include "grove/math/matrix_transform.hpp"
#include "grove/math/string_cast.hpp"
#include "grove/math/util.hpp"
#include "grove/common/common.hpp"
#include <iostream>

GROVE_NAMESPACE_BEGIN

FirstPersonCamera::FirstPersonCamera() :
  front(0.0f, 0.0f, -1.0f),
  position(0.0f),
  view(1.0f),
  projection(1.0f) {
  //
}

void FirstPersonCamera::move(const Vec3f& deltas) {
  position += deltas;
}

void FirstPersonCamera::rotate(const Vec3f& deltas) {
  const float theta_eps = 0.05f;
  const float min_theta = theta_eps;
  const float max_theta = grove::pif() - theta_eps;

  front = spherical_rotate(front, deltas.x, deltas.y, min_theta, max_theta);
}

void FirstPersonCamera::set_projection_info(const ProjectionInfo& info) {
  projection_info = info;
}

void FirstPersonCamera::set_position(const Vec3f& pos) {
  position = pos;
}

Vec3f FirstPersonCamera::get_front() const {
  return front;
}

void FirstPersonCamera::set_front(const Vec3f& v) {
  front = v;
}

Vec3f FirstPersonCamera::get_front_xz() const {
  auto f = front;
  f.y = 0.0f;
  return normalize(f);
}

Vec3f FirstPersonCamera::get_right() const {
  return Vec3f(view(0, 0), view(0, 1), view(0, 2));
}

Vec3f FirstPersonCamera::get_position() const {
  return position;
}

ProjectionInfo FirstPersonCamera::get_projection_info() const {
  return projection_info;
}

Mat4f FirstPersonCamera::get_projection() const {
  return projection;
}

Mat4f FirstPersonCamera::get_view() const {
  return view;
}

void FirstPersonCamera::update_view() {
  view = look_at(position, position + front, Vec3f(0.0f, 1.0f, 0.0f));
}

void FirstPersonCamera::update_projection() {
  projection = projection_from_info(projection_info);
}

GROVE_NAMESPACE_END
