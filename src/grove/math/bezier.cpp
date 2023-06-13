#include "bezier.hpp"
#include "Mat4.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

Vec4f parameter_vector(float t) {
  return {1.0f, t, t*t, t*t*t};
}

constexpr Mat4f cubic_bezier_basis_matrix() {
  return {1.0f, -3.0f, 3.0f, -1.0f,
          0.0f, 3.0f, -6.0f, 3.0f,
          0.0f, 0.0f, 3.0f, -3.0f,
          0.0f, 0.0f, 0.0f, 1.0f};
}

} //  anon

Vec3f CubicBezierCurvePoints::evaluate(float t) const {
  auto mt = cubic_bezier_basis_matrix() * parameter_vector(t);

  auto p0t = p0 * mt.x;
  auto p1t = p1 * mt.y;
  auto p2t = p2 * mt.z;
  auto p3t = p3 * mt.w;

  return p0t + p1t + p2t + p3t;
}

GROVE_NAMESPACE_END
