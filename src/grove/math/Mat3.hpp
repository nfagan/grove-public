#pragma once

#include "Vec3.hpp"

namespace grove {

template <typename T>
struct Mat3 {
  constexpr Mat3() = default;

  constexpr explicit Mat3(T diag);
  constexpr explicit Mat3(const Vec3<T>& a, const Vec3<T>& b, const Vec3<T>& c);
  constexpr explicit Mat3(T m00, T m01, T m02,
                          T m10, T m11, T m12,
                          T m20, T m21, T m22);

  const T& operator()(int r, int c) const;
  T& operator()(int r, int c);

  const Vec3<T>& operator[](int c) const;
  Vec3<T>& operator[](int c);

public:
  T elements[9];
};

template <typename T>
constexpr Mat3<T>::Mat3(T diag) :
  elements{diag, T(0), T(0),
           T(0), diag, T(0),
           T(0), T(0), diag} {
  //
}

template <typename T>
constexpr Mat3<T>::Mat3(const Vec3<T>& a, const Vec3<T>& b, const Vec3<T>& c) :
  elements{a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z} {
  //
}

template <typename T>
constexpr Mat3<T>::Mat3(T m00, T m01, T m02,
                        T m10, T m11, T m12,
                        T m20, T m21, T m22) :
  elements{m00, m10, m20, m01, m11, m21, m02, m12, m22} {
  //
}

template <typename T>
T& Mat3<T>::operator()(int r, int c) {
  assert(r >= 0 && r < 3 && c >= 0 && c < 3);
  return elements[c * 3 + r];
}

template <typename T>
const T& Mat3<T>::operator()(int r, int c) const {
  assert(r >= 0 && r < 3 && c >= 0 && c < 3);
  return elements[c * 3 + r];
}

template <typename T>
Vec3<T>& Mat3<T>::operator[](int col) {
  assert(col >= 0 && col < 3);
  return *reinterpret_cast<Vec3<T>*>(elements + col * 3);
}

template <typename T>
const Vec3<T>& Mat3<T>::operator[](int col) const {
  assert(col >= 0 && col < 3);
  return *reinterpret_cast<const Vec3<T>*>(elements + col * 3);
}

template <typename T>
Mat3<T> operator+(const Mat3<T>& a, const Mat3<T>& b) {
  Mat3<T> r;
  for (int i = 0; i < 9; i++) {
    r.elements[i] = a.elements[i] + b.elements[i];
  }
  return r;
}

template <typename T>
Mat3<T> operator-(const Mat3<T>& a, const Mat3<T>& b) {
  Mat3<T> r;
  for (int i = 0; i < 9; i++) {
    r.elements[i] = a.elements[i] - b.elements[i];
  }
  return r;
}

template <typename T>
Mat3<T> operator*(const Mat3<T>& a, const Mat3<T>& b) {
  Mat3<T> res;

  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      T element{0};

      for (int k = 0; k < 3; k++) {
        const T ak = a.elements[i + k * 3];
        const T bk = b.elements[k + j * 3];
        element += ak * bk;
      }

      res.elements[i + j * 3] = element;
    }
  }

  return res;
}

template <typename T>
Vec3<T> operator*(const Mat3<T>& a, const Vec3<T>& b) {
  return Vec3<T>{
    a.elements[0] * b.x + a.elements[3] * b.y + a.elements[6] * b.z,
    a.elements[1] * b.x + a.elements[4] * b.y + a.elements[7] * b.z,
    a.elements[2] * b.x + a.elements[5] * b.y + a.elements[8] * b.z,
  };
}

template <typename T>
Mat3<T> inverse(const Mat3<T>& m) {
  //  Due to Lengyel, E. Foundations of Game Engine Development Vol. 1, pp. 48
  const Vec3<T>& a = m[0];
  const Vec3<T>& b = m[1];
  const Vec3<T>& c = m[2];

  auto r0 = cross(b, c);
  auto r1 = cross(c, a);
  auto r2 = cross(a, b);
  auto inv_det = T(1) / dot(r2, c);

  r0 *= inv_det;
  r1 *= inv_det;
  r2 *= inv_det;

  return Mat3<T>{
    r0.x, r0.y, r0.z,
    r1.x, r1.y, r1.z,
    r2.x, r2.y, r2.z
  };
}

template <typename T>
Mat3<T> transpose(const Mat3<T>& m) {
  return Mat3<T>{
    m.elements[0], m.elements[1], m.elements[2],
    m.elements[3], m.elements[4], m.elements[5],
    m.elements[6], m.elements[7], m.elements[8]
  };
}

using Mat3f = Mat3<float>;

}