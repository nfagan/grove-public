#pragma once

#include "Vec2.hpp"
#include "Vec3.hpp"
#include <cmath>

namespace grove {

//  Express normalized direction vector in spherical coordinates (theta, phi). Theta is with
//  respect to the xz plane.
template <typename T>
Vec2<T> cartesian_to_spherical(const Vec3<T>& n) {
  return {std::acos(n.y), std::atan2(n.z, n.x)};
}

//  Convert spherical coordinates (theta, phi) to a normalized direction vector.
template <typename T>
Vec3<T> spherical_to_cartesian(const Vec2<T>& v) {
  const auto st = std::sin(v.x);

  Vec3<T> n{std::cos(v.y) * st,
            std::cos(v.x),
            std::sin(v.y) * st};

  return normalize(n);
}

//  Create an orthonormal coordinate system whose up vector is `up`.
template <typename T>
void make_coordinate_system_y(const Vec3<T>& up, Vec3<T>* i, Vec3<T>* j, Vec3<T>* k,
                              T too_similar = T(0.999)) {
  auto x = Vec3<T>{T(1), T(0), T(0)};
  if (std::abs(dot(x, up)) > too_similar) {
    x = Vec3<T>{T(0), T(1), T(0)};
  }

  auto z = normalize(cross(up, x));
  x = cross(z, up);

  *i = x;
  *j = up;
  *k = z;
}

}