#pragma once

#include "grove/math/vector.hpp"
#include "grove/math/matrix.hpp"

namespace grove {

template <typename T>
struct TRS {
  Vec3<T> translation;
  Vec4<T> rotation; //  @TODO: quat
  Vec3<T> scale;

  static TRS<T> identity();
  static TRS<T> make_translation(const Vec3<T>& trans);
  static TRS<T> make_translation_scale(const Vec3<T>& trans, const Vec3<T>& scale);
};

template <typename T>
TRS<T> TRS<T>::make_translation(const Vec3<T>& trans) {
  return TRS<T>{
    trans,
    Vec4<T>(T(0)),
    Vec3<T>(T(1))
  };
}

template <typename T>
TRS<T> TRS<T>::make_translation_scale(const Vec3<T>& trans, const Vec3<T>& scale) {
  return TRS<T>{
    trans,
    Vec4<T>(T(0)),
    scale
  };
}

template <typename T>
TRS<T> TRS<T>::identity() {
  return TRS<T>{
    Vec3<T>(T(0)),
    Vec4<T>(T(0)),
    Vec3<T>(T(1))
  };
}

template <typename T>
TRS<T> inverse(const TRS<T>& a) {
  return TRS<T>{
    -a.translation,
    T(1) / a.rotation,  //  @TODO
    T(1) / a.scale
  };
}

template <typename T>
TRS<T> apply(const TRS<T>& a, const TRS<T>& b) {
  return TRS<T>{
    a.translation + b.translation,
    a.rotation * b.rotation,  //  @TODO
    a.scale * b.scale
  };
}

template <typename T>
Mat4<T> to_mat4(const TRS<T>& a) {
  Mat4<T> result(T(0));
  result.set_diagonal(a.scale.x, a.scale.y, a.scale.z, T(1));
  result.elements[12] = a.translation.x;
  result.elements[13] = a.translation.y;
  result.elements[14] = a.translation.z;
  //  @TODO: Include rotation
  return result;
}

}