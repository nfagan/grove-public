#pragma once

#include "Vec3.hpp"
#include "Vec4.hpp"

namespace grove {

struct Frustum {
  struct Planes {
    Vec4f near;
    Vec4f far;
    Vec4f left;
    Vec4f right;
    Vec4f top;
    Vec4f bottom;
  };

  union {
    Planes planes;
    Vec4f array[6];
  };
};

inline Frustum make_world_space_frustum(float s, float g, float n, float f, const Vec3f& v0,
                                        const Vec3f& v1, const Vec3f& v2, const Vec3f& t) {
  //  Lengyel, E. Foundations of Game Engine Development Vol 2., pp 57.
  auto ln = normalize(g * v0 + s * v2);
  auto rn = normalize(-g * v0 + s * v2);
  auto tn = normalize(g * v1 + v2);
  auto bn = normalize(-g * v1 + v2);
  Frustum result;
  result.planes.near = Vec4f{v2, -dot(v2, t + v2 * n)};
  result.planes.far = Vec4f{-v2, -dot(-v2, t + v2 * f)};
  result.planes.left = Vec4f{ln, -dot(ln, t)};
  result.planes.right = Vec4f{rn, -dot(rn, t)};
  result.planes.top = Vec4f{tn, -dot(tn, t)};
  result.planes.bottom = Vec4f{bn, -dot(bn, t)};
  return result;
}

inline Frustum make_camera_space_frustum(float s, float g, float n, float f) {
  //  Lengyel, E. Foundations of Game Engine Development Vol 2., pp 56-57.
  const auto gs_len = std::sqrt(g * g + s * s);
  const auto g1_len = sqrt(g * g + 1.0f);
  Frustum result;
  result.planes.near = Vec4f{0.0f, 0.0f, 1.0f, -n};
  result.planes.far = Vec4f{0.0f, 0.0f, -1.0f, f};
  result.planes.left = Vec4f{g, 0.0f, s, 0.0f} / gs_len;
  result.planes.right = Vec4f{-g, 0.0f, s, 0.0f} / gs_len;
  result.planes.top = Vec4f{0.0f, g, 1.0f, 0.0f} / g1_len;
  result.planes.bottom = Vec4f{0.0f, -g, 1.0f, 0.0f} / g1_len;
  return result;
}

}