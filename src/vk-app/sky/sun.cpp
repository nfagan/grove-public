#include "sun.hpp"
#include "grove/common/common.hpp"
#include "grove/math/constants.hpp"
#include <cmath>

GROVE_NAMESPACE_BEGIN

Vec3<double> sun::compute_position(double theta_01, double phi_radians, double distance) {
  phi_radians += (theta_01 >= 0.5 ? two_pi() : 0.0);
  const auto theta = pi() * theta_01;

  const auto cp = std::cos(phi_radians);
  const auto sp = std::sin(phi_radians);
  const auto st = std::sin(theta);
  const auto ct = std::cos(theta);

  auto x = cp * ct;
  auto y = st;
  auto z = -sp * ct;

  return Vec3<double>{x, y, z} * distance;
}

GROVE_NAMESPACE_END
