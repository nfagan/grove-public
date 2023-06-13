#pragma once

#include "Vec3.hpp"
#include "Vec4.hpp"

namespace grove {

template <typename T>
struct Mat3x4 {
  constexpr Mat3x4() = default;
  constexpr explicit Mat3x4(T diag);
  constexpr Mat3x4(T m00, T m01, T m02, T m03,
                   T m10, T m11, T m12, T m13,
                   T m20, T m21, T m22, T m23);
  constexpr Mat3x4(const Vec3<T>& a,
                   const Vec3<T>& b,
                   const Vec3<T>& c,
                   const Vec3<T>& d);

  Vec3<T>& operator[](int col);
  const Vec3<T>& operator[](int col) const;

  T& operator()(int r, int c);
  const T& operator()(int r, int c) const;

  T elements[12];
};

template <typename T>
constexpr Mat3x4<T>::Mat3x4(T diag) : Mat3x4(diag, T(0), T(0), T(0),
                                             T(0), diag, T(0), T(0),
                                             T(0), T(0), diag, T(0)) {
  //
}

template <typename T>
constexpr Mat3x4<T>::Mat3x4(T m00, T m01, T m02, T m03,
                            T m10, T m11, T m12, T m13,
                            T m20, T m21, T m22, T m23) :
                            elements{m00, m10, m20,
                                     m01, m11, m21,
                                     m02, m12, m22,
                                     m03, m13, m23} {
  //
}

template <typename T>
constexpr Mat3x4<T>::Mat3x4(const Vec3<T>& a,
                            const Vec3<T>& b,
                            const Vec3<T>& c,
                            const Vec3<T>& d) :
  elements{a.x, a.y, a.z,
           b.x, b.y, b.z,
           c.x, c.y, c.z,
           d.x, d.y, d.z} {
  //
}

template <typename T>
inline Vec3<T>& Mat3x4<T>::operator[](int col) {
  assert(col >= 0 && col < 4);
  return *reinterpret_cast<Vec3<T>*>(elements + col * 3);
}

template <typename T>
inline const Vec3<T>& Mat3x4<T>::operator[](int col) const {
  assert(col >= 0 && col < 4);
  return *reinterpret_cast<const Vec3<T>*>(elements + col * 3);
}

template <typename T>
inline T& Mat3x4<T>::operator()(int r, int c) {
  assert(r >= 0 && r < 3 && c >= 0 && c < 4);
  return elements[c * 3 + r];
}

template <typename T>
inline const T& Mat3x4<T>::operator()(int r, int c) const {
  assert(r >= 0 && r < 3 && c >= 0 && c < 4);
  return elements[c * 3 + r];
}

template <typename T>
inline Vec3<T> operator*(const Mat3x4<T>& a, const Vec4<T>& b) {
  return {
    a.elements[0] * b.x + a.elements[3] * b.y + a.elements[6] * b.z + a.elements[9] * b.w,
    a.elements[1] * b.x + a.elements[4] * b.y + a.elements[7] * b.z + a.elements[10] * b.w,
    a.elements[2] * b.x + a.elements[5] * b.y + a.elements[8] * b.z + a.elements[11] * b.w
  };
}

template <typename T>
inline Mat3x4<T> inverse_implicit_unit_row(const Mat3x4<T>& m) {
  //  Inverts the 4x4 matrix that is implicitly `m` augmented with a
  //  4th row [T(0), T(0), T(0), T(1)].
  //  Adapted from Lengyel, E. Foundations of Game Engine Development Vol. 1, pp. 50
  Vec3<T> a{m.elements[0], m.elements[1], m.elements[2]};
  Vec3<T> b{m.elements[3], m.elements[4], m.elements[5]};
  Vec3<T> c{m.elements[6], m.elements[7], m.elements[8]};
  Vec3<T> d{m.elements[9], m.elements[10], m.elements[11]};

  auto s = grove::cross(a, b);
  auto t = grove::cross(c, d);

  auto inv_det = T(1) / grove::dot(s, c);
  s *= inv_det;
  t *= inv_det;
  c *= inv_det;

  auto r0 = cross(b, c);
  auto r1 = cross(c, a);
  auto r2 = s;

  auto r0w = -grove::dot(b, t);
  auto r1w = grove::dot(a, t);
  auto r2w = -grove::dot(d, s);

  return Mat3x4<T>{
    r0.x, r0.y, r0.z, r0w,
    r1.x, r1.y, r1.z, r1w,
    r2.x, r2.y, r2.z, r2w
  };
}

using Mat3x4f = Mat3x4<float>;

}