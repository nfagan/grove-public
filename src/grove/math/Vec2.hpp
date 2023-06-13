#pragma once

#include <cmath>
#include <cassert>
#include <algorithm>

namespace grove {

template <typename T>
struct Vec2 {
  struct Less {
    inline bool operator()(const Vec2<T>& a, const Vec2<T>& b) const noexcept {
      return a.x < b.x || (a.x == b.x && a.y < b.y);
    }
  };

  constexpr Vec2() = default;
  constexpr explicit Vec2(T c);
  constexpr Vec2(T x, T y);

  Vec2<T>& operator+=(T v);
  Vec2<T>& operator-=(T v);
  Vec2<T>& operator*=(T v);
  Vec2<T>& operator/=(T v);

  Vec2<T>& operator+=(const Vec2<T>& v);
  Vec2<T>& operator-=(const Vec2<T>& v);
  Vec2<T>& operator*=(const Vec2<T>& v);
  Vec2<T>& operator/=(const Vec2<T>& v);

  T& operator[](int idx);
  const T& operator[](int idx) const;

  T length_squared() const;
  T length() const;
  void normalize();

  T x;
  T y;
};

/*
* Impl
*/

template <typename T>
constexpr Vec2<T>::Vec2(T x, T y) : x(x), y(y) {
  //
}

template <typename T>
constexpr Vec2<T>::Vec2(T c) : x(c), y(c) {
  //
}

//  Member functions
template <typename T>
T Vec2<T>::length() const {
  return std::sqrt(length_squared());
}

template <typename T>
T Vec2<T>::length_squared() const {
  return x * x + y * y;
}

template <typename T>
void Vec2<T>::normalize() {
  auto inv_len = T(1) / length();
  x *= inv_len;
  y *= inv_len;
}

template <typename T>
T& Vec2<T>::operator[](int idx) {
  switch (idx) {
    case 0:
      return x;
    case 1:
      return y;
    default:
      assert(false);
      return x;
  }
}

template <typename T>
const T& Vec2<T>::operator[](int idx) const {
  switch (idx) {
    case 0:
      return x;
    case 1:
      return y;
    default:
      assert(false);
      return x;
  }
}

//  Member ops
template <typename T>
Vec2<T>& Vec2<T>::operator+=(T v) {
  x += v;
  y += v;
  return *this;
}

template <typename T>
Vec2<T>& Vec2<T>::operator-=(T v) {
  x -= v;
  y -= v;
  return *this;
}

template <typename T>
Vec2<T>& Vec2<T>::operator*=(T v) {
  x *= v;
  y *= v;
  return *this;
}

template <typename T>
Vec2<T>& Vec2<T>::operator/=(T v) {
  x /= v;
  y /= v;
  return *this;
}

//  Vec other
template <typename T>
Vec2<T>& Vec2<T>::operator+=(const Vec2<T>& v) {
  x += v.x;
  y += v.y;
  return *this;
}

template <typename T>
Vec2<T>& Vec2<T>::operator-=(const Vec2<T>& v) {
  x -= v.x;
  y -= v.y;
  return *this;
}

template <typename T>
Vec2<T>& Vec2<T>::operator*=(const Vec2<T>& v) {
  x *= v.x;
  y *= v.y;
  return *this;
}

template <typename T>
Vec2<T>& Vec2<T>::operator/=(const Vec2<T>& v) {
  x /= v.x;
  y /= v.y;
  return *this;
}

//  (vec, vec)
template <typename T>
inline Vec2<T> operator+(const Vec2<T>& a, const Vec2<T>& b) {
  return Vec2<T>(a.x + b.x, a.y + b.y);
}

template <typename T>
inline Vec2<T> operator-(const Vec2<T>& a, const Vec2<T>& b) {
  return Vec2<T>(a.x - b.x, a.y - b.y);
}

template <typename T>
inline Vec2<T> operator*(const Vec2<T>& a, const Vec2<T>& b) {
  return Vec2<T>(a.x * b.x, a.y * b.y);
}

template <typename T>
inline Vec2<T> operator/(const Vec2<T>& a, const Vec2<T>& b) {
  return Vec2<T>(a.x / b.x, a.y / b.y);
}

template <typename T>
inline Vec2<T> operator-(const Vec2<T>& a) {
  return Vec2<T>(-a.x, -a.y);
}

//  (T, vec)
template <typename T>
inline Vec2<T> operator+(T v, const Vec2<T>& a) {
  return Vec2<T>(v + a.x, v + a.y);
}

template <typename T>
inline Vec2<T> operator-(T v, const Vec2<T>& a) {
  return Vec2<T>(v - a.x, v - a.y);
}

template <typename T>
inline Vec2<T> operator*(T v, const Vec2<T>& a) {
  return Vec2<T>(v * a.x, v * a.y);
}

template <typename T>
inline Vec2<T> operator/(T v, const Vec2<T>& a) {
  return Vec2<T>(v / a.x, v / a.y);
}

//  (vec, T)
template <typename T>
inline Vec2<T> operator+(const Vec2<T>& a, T v) {
  return Vec2<T>(a.x + v, a.y + v);
}

template <typename T>
inline Vec2<T> operator-(const Vec2<T>& a, T v) {
  return Vec2<T>(a.x - v, a.y - v);
}

template <typename T>
inline Vec2<T> operator*(const Vec2<T>& a, T v) {
  return Vec2<T>(a.x * v, a.y * v);
}

template <typename T>
inline Vec2<T> operator/(const Vec2<T>& a, T v) {
  return Vec2<T>(a.x / v, a.y / v);
}

template <typename T>
inline bool operator==(const Vec2<T>& a, const Vec2<T>& b) {
  return a.x == b.x && a.y == b.y;
}

template <typename T>
inline bool operator!=(const Vec2<T>& a, const Vec2<T>& b) {
  return !(a == b);
}

//  Util
template <typename T>
Vec2<T> normalize(const Vec2<T>& a) {
  auto inv_len = T(1) / a.length();
  return Vec2<T>(a.x * inv_len, a.y * inv_len);
}

template <typename T>
T dot(const Vec2<T>& a, const Vec2<T>& b) {
  return a.x * b.x + a.y * b.y;
}

template <typename T>
Vec2<T> min(const Vec2<T>& a, const Vec2<T>& b) {
  return Vec2<T>(std::min(a.x, b.x),
                 std::min(a.y, b.y));
}

template <typename T>
Vec2<T> max(const Vec2<T>& a, const Vec2<T>& b) {
  return Vec2<T>(std::max(a.x, b.x),
                 std::max(a.y, b.y));
}

template <typename T>
Vec2<T> floor(const Vec2<T>& a) {
  return Vec2<T>(std::floor(a.x), std::floor(a.y));
}

template <typename T>
Vec2<T> round(const Vec2<T>& a) {
  return Vec2<T>(std::round(a.x), std::round(a.y));
}

template <typename T>
Vec2<T> abs(const Vec2<T>& a) {
  return Vec2<T>(std::abs(a.x), std::abs(a.y));
}

template <typename T>
bool any(const Vec2<T>& a) {
  return bool(a.x) || bool(a.y);
}

template <typename T>
bool all(const Vec2<T>& a) {
  return bool(a.x) && bool(a.y);
}

template <typename T>
Vec2<bool> lt(const Vec2<T>& a, const Vec2<T>& b) {
  return Vec2<bool>{a.x < b.x, a.y < b.y};
}

template <typename T>
Vec2<bool> le(const Vec2<T>& a, const Vec2<T>& b) {
  return Vec2<bool>{a.x <= b.x, a.y <= b.y};
}

template <typename T>
Vec2<bool> gt(const Vec2<T>& a, const Vec2<T>& b) {
  return Vec2<bool>{a.x > b.x, a.y > b.y};
}

template <typename T>
Vec2<bool> ge(const Vec2<T>& a, const Vec2<T>& b) {
  return Vec2<bool>{a.x >= b.x, a.y >= b.y};
}

template <typename T>
Vec2<bool> eq(const Vec2<T>& a, const Vec2<T>& b) {
  return Vec2<bool>{a.x == b.x, a.y == b.y};
}

template <typename T>
Vec2<bool> ne(const Vec2<T>& a, const Vec2<T>& b) {
  return Vec2<bool>{a.x != b.x, a.y != b.y};
}

template <typename T>
Vec2<T> clamp_each(const Vec2<T>& a, const Vec2<T>& min, const Vec2<T>& max) {
  return Vec2<T>(a.x < min.x ? min.x : a.x > max.x ? max.x : a.x,
                 a.y < min.y ? min.y : a.y > max.y ? max.y : a.y);
}

template <typename T>
T det3_implicit(const Vec2<T>& p0, const Vec2<T>& p1, const Vec2<T>& p2) {
  //  Determinant of 3x3 matrix [p0, p1, p2] with a last row of [1]
  T a = p0.x;
  T b = p0.y;
  T c = p1.x;
  T d = p1.y;
  T e = p2.x;
  T f = p2.y;
  return a * (d - f) - c * (b - f) + e * (b - d);
}

//  Global typedefs
using Vec2f = Vec2<float>;

}