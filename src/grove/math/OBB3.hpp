#pragma once

#include "Vec3.hpp"

namespace grove {

template <typename T>
struct OBB3 {
  Vec3<T> i;
  Vec3<T> j;
  Vec3<T> k;
  Vec3<T> position;
  Vec3<T> half_size;

  static OBB3<T> axis_aligned(const Vec3<T>& p, const Vec3<T>& sz) {
    return OBB3<T>{
      ConstVec3<T>::positive_x,
      ConstVec3<T>::positive_y,
      ConstVec3<T>::positive_z,
      p, sz
    };
  }

  friend inline bool operator==(const OBB3<T>& a, const OBB3<T>& b) {
    return a.i == b.i && a.j == b.j && a.k == b.k &&
           a.position == b.position && a.half_size == b.half_size;
  }

  friend inline bool operator!=(const OBB3<T>& a, const OBB3<T>& b) {
    return !(a == b);
  }
};

using OBB3f = OBB3<float>;

template <typename T>
Vec3<T> orient(const OBB3<T>& b, const Vec3<T>& v) {
  return v.x * b.i + v.y * b.j + v.z * b.k;
}

template <typename T>
void gather_vertices(const OBB3<T>& obb, Vec3<T>* out) {
  int ind{};
  out[ind++] = Vec3<T>{T(-1), T(-1), T(-1)};
  out[ind++] = Vec3<T>{T(1), T(-1), T(-1)};
  out[ind++] = Vec3<T>{T(1), T(1), T(-1)};
  out[ind++] = Vec3<T>{T(-1), T(1), T(-1)};
  //
  out[ind++] = Vec3<T>{T(-1), T(-1), T(1)};
  out[ind++] = Vec3<T>{T(1), T(-1), T(1)};
  out[ind++] = Vec3<T>{T(1), T(1), T(1)};
  out[ind++] = Vec3<T>{T(-1), T(1), T(1)};
  for (int i = 0; i < 8; i++) {
    auto& v = out[i];
    v *= obb.half_size;
    v = obb.i * v.x + obb.j * v.y + obb.k * v.z;
    v += obb.position;
  }
}

}