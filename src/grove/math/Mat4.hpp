#pragma once

#include "Vec4.hpp"
#include "Vec3.hpp"
#include <cstring>
#include <cassert>

namespace grove {

template <typename T>
struct Vec4;

template <typename T>
struct Mat4 {
  constexpr Mat4() = default;
  constexpr explicit Mat4(T diag);
  constexpr Mat4(T m00, T m01, T m02, T m03,
                 T m10, T m11, T m12, T m13,
                 T m20, T m21, T m22, T m23,
                 T m30, T m31, T m32, T m33);

  T& operator()(int i, int j);
  const T& operator()(int i, int j) const;

  Vec4<T>& operator[](int col);
  const Vec4<T>& operator[](int col) const;

  Vec4<T> row(int i) const;

  void set_diagonal(T val);
  void set_diagonal(T a, T b, T c, T d);
  void set_diagonal(const Vec4<T>& val);
  Vec4<T> get_diagonal() const;

  void identity();

  T elements[16];
};

/*
 * Impl
 */

//  Ctor
template <typename T>
constexpr Mat4<T>::Mat4(T diag) :
  elements{diag, T(0), T(0), T(0),
           T(0), diag, T(0), T(0),
           T(0), T(0), diag, T(0),
           T(0), T(0), T(0), diag} {
  //
}

template <typename T>
constexpr Mat4<T>::Mat4(T m00, T m01, T m02, T m03,
                        T m10, T m11, T m12, T m13,
                        T m20, T m21, T m22, T m23,
                        T m30, T m31, T m32, T m33) :
                        elements{m00, m10, m20, m30,
                                 m01, m11, m21, m31,
                                 m02, m12, m22, m32,
                                 m03, m13, m23, m33} {
  //
}

//  Members
template <typename T>
void Mat4<T>::identity() {
  std::memset(&elements[0], 0, 16 * sizeof(T));
  set_diagonal(T(1));
}

template <typename T>
void Mat4<T>::set_diagonal(T val) {
  elements[0] = val;
  elements[5] = val;
  elements[10] = val;
  elements[15] = val;
}

template <typename T>
void Mat4<T>::set_diagonal(T a, T b, T c, T d) {
  elements[0] = a;
  elements[5] = b;
  elements[10] = c;
  elements[15] = d;
}

template <typename T>
void Mat4<T>::set_diagonal(const Vec4<T>& val) {
  elements[0] = val.x;
  elements[5] = val.y;
  elements[10] = val.z;
  elements[15] = val.w;
}

template <typename T>
Vec4<T> Mat4<T>::get_diagonal() const {
  return Vec4<T>{elements[0], elements[5], elements[10], elements[15]};
}

template <typename T>
T& Mat4<T>::operator()(int i, int j) {
  assert(i >= 0 && i < 4 && j >= 0 && j < 4);
  return elements[j * 4 + i];
}

template <typename T>
const T& Mat4<T>::operator()(int i, int j) const {
  assert(i >= 0 && i < 4 && j >= 0 && j < 4);
  return elements[j * 4 + i];
}

template <typename T>
Vec4<T>& Mat4<T>::operator[](int col) {
  return *reinterpret_cast<Vec4<T>*>(elements + col * 4);
}

template <typename T>
const Vec4<T>& Mat4<T>::operator[](int col) const {
  return *reinterpret_cast<const Vec4<T>*>(elements + col * 4);
}

template <typename T>
Vec4<T> Mat4<T>::row(int i) const {
  assert(i >= 0 && i < 4);
  const T x = elements[i];
  const T y = elements[4 + i];
  const T z = elements[8 + i];
  const T w = elements[12 + i];

  return Vec4<T>(x, y, z, w);
}

//  Ops
template <typename T>
Mat4<T> operator+(const Mat4<T>& a, const Mat4<T>& b) {
  Mat4<T> res;
  for (int i = 0; i < 16; i++) {
    res.elements[i] = a.elements[i] + b.elements[i];
  }
  return res;
}

template <typename T>
Mat4<T> operator-(const Mat4<T>& a, const Mat4<T>& b) {
  Mat4<T> res;
  for (int i = 0; i < 16; i++) {
    res.elements[i] = a.elements[i] - b.elements[i];
  }
  return res;
}

template <typename T>
Mat4<T> operator*(const Mat4<T>& a, const Mat4<T>& b) {
  Mat4<T> res;

  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      T element{0};

      for (int k = 0; k < 4; k++) {
        const T ak = a.elements[i + k * 4];
        const T bk = b.elements[k + j * 4];
        element += ak * bk;
      }

      res.elements[i + j * 4] = element;
    }
  }

  return res;
}

template <typename T>
Vec4<T> operator*(const Mat4<T>& a, const Vec4<T>& b) {
  Vec4<T> res;

  const T vxx = a.elements[0];
  const T vxy = a.elements[4];
  const T vxz = a.elements[8];
  const T vxw = a.elements[12];

  const T vyx = a.elements[1];
  const T vyy = a.elements[5];
  const T vyz = a.elements[9];
  const T vyw = a.elements[13];

  const T vzx = a.elements[2];
  const T vzy = a.elements[6];
  const T vzz = a.elements[10];
  const T vzw = a.elements[14];

  const T vwx = a.elements[3];
  const T vwy = a.elements[7];
  const T vwz = a.elements[11];
  const T vww = a.elements[15];

  res.x = vxx * b.x + vxy * b.y + vxz * b.z + vxw * b.w;
  res.y = vyx * b.x + vyy * b.y + vyz * b.z + vyw * b.w;
  res.z = vzx * b.x + vzy * b.y + vzz * b.z + vzw * b.w;
  res.w = vwx * b.x + vwy * b.y + vwz * b.z + vww * b.w;

  return res;
}

//  Inlines
template <typename T>
Mat4<T> transpose(const Mat4<T>& a) {
  Mat4<T> b;

  b.elements[0] = a.elements[0];
  b.elements[1] = a.elements[4];
  b.elements[2] = a.elements[8];
  b.elements[3] = a.elements[12];

  b.elements[4] = a.elements[1];
  b.elements[5] = a.elements[5];
  b.elements[6] = a.elements[9];
  b.elements[7] = a.elements[13];

  b.elements[8] = a.elements[2];
  b.elements[9] = a.elements[6];
  b.elements[10] = a.elements[10];
  b.elements[11] = a.elements[14];

  b.elements[12] = a.elements[3];
  b.elements[13] = a.elements[7];
  b.elements[14] = a.elements[11];
  b.elements[15] = a.elements[15];

  return b;
}

template <typename T>
Mat4<T> inverse(const Mat4<T>& m) {
  //  Due to Lengyel, E. Foundations of Game Engine Development Vol. 1, pp. 50
  const T& x = m(3, 0);
  const T& y = m(3, 1);
  const T& z = m(3, 2);
  const T& w = m(3, 3);

  auto a = Vec3<T>(m.elements[0], m.elements[1], m.elements[2]);
  auto b = Vec3<T>(m.elements[4], m.elements[5], m.elements[6]);
  auto c = Vec3<T>(m.elements[8], m.elements[9], m.elements[10]);
  auto d = Vec3<T>(m.elements[12], m.elements[13], m.elements[14]);

  auto s = grove::cross(a, b);
  auto t = grove::cross(c, d);
  auto u = a * y - b * x;
  auto v = c * w - d * z;

  T inv_det = T(1) / (grove::dot(s, v) + grove::dot(t, u));
  s *= inv_det;
  t *= inv_det;
  u *= inv_det;
  v *= inv_det;

  auto r0 = grove::cross(b, v) + t * y;
  auto r1 = grove::cross(v, a) - t * x;
  auto r2 = grove::cross(d, u) + s * w;
  auto r3 = grove::cross(u, c) - s * z;

  auto r0w = -grove::dot(b, t);
  auto r1w = grove::dot(a, t);
  auto r2w = -grove::dot(d, s);
  auto r3w = grove::dot(c, s);

  return Mat4<T>(r0.x, r0.y, r0.z, r0w,
                 r1.x, r1.y, r1.z, r1w,
                 r2.x, r2.y, r2.z, r2w,
                 r3.x, r3.y, r3.z, r3w);
}

//  Global typedefs
using Mat4f = Mat4<float>;

}