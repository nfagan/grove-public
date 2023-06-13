#pragma once

#include "Bounds3.hpp"
#include "OBB3.hpp"

namespace grove {

template <typename T>
Bounds3<T> obb3_to_aabb(const OBB3<T>& a) {
  Vec3<T> vs[8];
  gather_vertices(a, vs);
  Bounds3<T> result;
  union_of(vs, 8, &result.min, &result.max);
  return result;
}

}