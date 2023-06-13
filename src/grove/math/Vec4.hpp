#pragma once

#include <cmath>
#include <cassert>
#include <algorithm>

namespace grove {

template <typename T>
struct Vec3;

template <typename T>
struct Vec4 {
  constexpr Vec4() = default;
  constexpr explicit Vec4(T c);
  constexpr Vec4(T x, T y, T z, T w);
  constexpr Vec4(const Vec3<T>& v, T c);

  Vec4<T>& operator+=(T v);
  Vec4<T>& operator-=(T v);
  Vec4<T>& operator*=(T v);
  Vec4<T>& operator/=(T v);

  Vec4<T>& operator+=(const Vec4<T>& v);
  Vec4<T>& operator-=(const Vec4<T>& v);
  Vec4<T>& operator*=(const Vec4<T>& v);
  Vec4<T>& operator/=(const Vec4<T>& v);

  T& operator[](int idx);
  const T& operator[](int idx) const;

  T length_squared() const;
  T length() const;
  void normalize();

  T x;
  T y;
  T z;
  T w;
};

/*
* Impl
*/

template <typename T>
constexpr Vec4<T>::Vec4(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {
  //
}

template <typename T>
constexpr Vec4<T>::Vec4(T c) : x(c), y(c), z(c), w(c) {
  //
}

template <typename T>
constexpr Vec4<T>::Vec4(const Vec3<T>& v, T c) : x(v.x), y(v.y), z(v.z), w(c) {
  //
}

//  Member functions
template <typename T>
T Vec4<T>::length() const {
  return std::sqrt(length_squared());
}

template <typename T>
T Vec4<T>::length_squared() const {
  return x * x + y * y + z * z + w * w;
}

template <typename T>
void Vec4<T>::normalize() {
  auto inv_len = T(1) / length();
  x *= inv_len;
  y *= inv_len;
  z *= inv_len;
  w *= inv_len;
}

template <typename T>
T& Vec4<T>::operator[](int idx) {
  switch (idx) {
    case 0:
      return x;
    case 1:
      return y;
    case 2:
      return z;
    case 3:
      return w;
    default:
      assert(false);
      return x;
  }
}

template <typename T>
const T& Vec4<T>::operator[](int idx) const {
  switch (idx) {
    case 0:
      return x;
    case 1:
      return y;
    case 2:
      return z;
    case 3:
      return w;
    default:
      assert(false);
      return x;
  }
}

//  Member ops
template <typename T>
Vec4<T>& Vec4<T>::operator+=(T v) {
  x += v;
  y += v;
  z += v;
  w += v;
  return *this;
}

template <typename T>
Vec4<T>& Vec4<T>::operator-=(T v) {
  x -= v;
  y -= v;
  z -= v;
  w -= v;
  return *this;
}

template <typename T>
Vec4<T>& Vec4<T>::operator*=(T v) {
  x *= v;
  y *= v;
  z *= v;
  w *= v;
  return *this;
}

template <typename T>
Vec4<T>& Vec4<T>::operator/=(T v) {
  x /= v;
  y /= v;
  z /= v;
  w /= v;
  return *this;
}

//  Vec other
template <typename T>
Vec4<T>& Vec4<T>::operator+=(const Vec4<T>& v) {
  x += v.x;
  y += v.y;
  z += v.z;
  w += v.w;
  return *this;
}

template <typename T>
Vec4<T>& Vec4<T>::operator-=(const Vec4<T>& v) {
  x -= v.x;
  y -= v.y;
  z -= v.z;
  w -= v.w;
  return *this;
}

template <typename T>
Vec4<T>& Vec4<T>::operator*=(const Vec4<T>& v) {
  x *= v.x;
  y *= v.y;
  z *= v.z;
  w *= v.w;
  return *this;
}

template <typename T>
Vec4<T>& Vec4<T>::operator/=(const Vec4<T>& v) {
  x /= v.x;
  y /= v.y;
  z /= v.z;
  w /= v.w;
  return *this;
}

//  (vec, vec)
template <typename T>
inline Vec4<T> operator+(const Vec4<T>& a, const Vec4<T>& b) {
  return Vec4<T>(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

template <typename T>
inline Vec4<T> operator-(const Vec4<T>& a, const Vec4<T>& b) {
  return Vec4<T>(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

template <typename T>
inline Vec4<T> operator*(const Vec4<T>& a, const Vec4<T>& b) {
  return Vec4<T>(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
}

template <typename T>
inline Vec4<T> operator/(const Vec4<T>& a, const Vec4<T>& b) {
  return Vec4<T>(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w);
}

//  (vec, T)
template <typename T>
inline Vec4<T> operator+(const Vec4<T>& a, T b) {
  return Vec4<T>(a.x + b, a.y + b, a.z + b, a.w + b);
}

template <typename T>
inline Vec4<T> operator-(const Vec4<T>& a, T b) {
  return Vec4<T>(a.x - b, a.y - b, a.z - b, a.w - b);
}

template <typename T>
inline Vec4<T> operator*(const Vec4<T>& a, T b) {
  return Vec4<T>(a.x * b, a.y * b, a.z * b, a.w * b);
}

template <typename T>
inline Vec4<T> operator/(const Vec4<T>& a, T b) {
  return Vec4<T>(a.x / b, a.y / b, a.z / b, a.w / b);
}

//  (T, vec)
template <typename T>
inline Vec4<T> operator+(T a, const Vec4<T>& b) {
  return Vec4<T>(a + b.x, a + b.y, a + b.z, a + b.w);
}

template <typename T>
inline Vec4<T> operator-(T a, const Vec4<T>& b) {
  return Vec4<T>(a - b.x, a - b.y, a - b.z, a - b.w);
}

template <typename T>
inline Vec4<T> operator*(T a, const Vec4<T>& b) {
  return Vec4<T>(a * b.x, a * b.y, a * b.z, a * b.w);
}

template <typename T>
inline Vec4<T> operator/(T a, const Vec4<T>& b) {
  return Vec4<T>(a / b.x, a / b.y, a / b.z, a / b.w);
}

template <typename T>
inline Vec4<T> operator-(const Vec4<T>& a) {
  return Vec4<T>(-a.x, -a.y, -a.z, -a.w);
}

//  Util
template <typename T>
Vec4<T> normalize(const Vec4<T>& a) {
  auto inv_len = T(1) / a.length();
  return Vec4<T>(a.x * inv_len, a.y * inv_len, a.z * inv_len, a.w * inv_len);
}

template <typename T>
T dot(const Vec4<T>& a, const Vec4<T>& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

template <typename T>
Vec4<T> min(const Vec4<T>& a, const Vec4<T>& b) {
  return Vec4<T>(std::min(a.x, b.x),
                 std::min(a.y, b.y),
                 std::min(a.z, b.z),
                 std::min(a.w, b.w));
}

template <typename T>
Vec4<T> max(const Vec4<T>& a, const Vec4<T>& b) {
  return Vec4<T>(std::max(a.x, b.x),
                 std::max(a.y, b.y),
                 std::max(a.z, b.z),
                 std::max(a.w, b.w));
}

template <typename T>
Vec4<T> clamp_each(const Vec4<T>& a, const Vec4<T>& min, const Vec4<T>& max) {
  return Vec4<T>(a.x < min.x ? min.x : a.x > max.x ? max.x : a.x,
                 a.y < min.y ? min.y : a.y > max.y ? max.y : a.y,
                 a.z < min.z ? min.z : a.z > max.z ? max.z : a.z,
                 a.w < min.w ? min.w : a.w > max.w ? max.w : a.w);
}

template <typename T>
Vec3<T> to_vec3(const Vec4<T>& a) {
  return Vec3<T>(a.x, a.y, a.z);
}

//  Global typedefs
using Vec4f = Vec4<float>;

}