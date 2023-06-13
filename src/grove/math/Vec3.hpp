#pragma once

#include <cmath>
#include <cassert>
#include <algorithm>

namespace grove {

template <typename T>
struct Vec2;

template <typename T>
struct Vec3 {
  struct Less {
    inline bool operator()(const Vec3<T>& a, const Vec3<T>& b) const noexcept {
      return std::tie(a.x, a.y, a.z) < std::tie(b.x, b.y, b.z);
    }
  };

  Vec3() = default;
  constexpr explicit Vec3(T c);
  constexpr Vec3(T x, T y, T z);
  Vec3(const Vec2<T>& v, T c);

  Vec3<T>& operator+=(T v);
  Vec3<T>& operator-=(T v);
  Vec3<T>& operator*=(T v);
  Vec3<T>& operator/=(T v);

  Vec3<T>& operator+=(const Vec3<T>& v);
  Vec3<T>& operator-=(const Vec3<T>& v);
  Vec3<T>& operator*=(const Vec3<T>& v);
  Vec3<T>& operator/=(const Vec3<T>& v);

  T& operator[](int idx);
  const T& operator[](int idx) const;

  T length_squared() const;
  T length() const;
  void normalize();

  T x;
  T y;
  T z;
};

template <typename T>
struct ConstVec3 {
  static constexpr Vec3<T> positive_x{T(1), T(0), T(0)};
  static constexpr Vec3<T> positive_y{T(0), T(1), T(0)};
  static constexpr Vec3<T> positive_z{T(0), T(0), T(1)};
};

/*
 * Impl
 */

template <typename T>
constexpr Vec3<T>::Vec3(T x, T y, T z) : x(x), y(y), z(z) {
  //
}

template <typename T>
constexpr Vec3<T>::Vec3(T c) : x(c), y(c), z(c) {
  //
}

template <typename T>
Vec3<T>::Vec3(const Vec2<T>& v, T c) : x(v.x), y(v.y), z(c) {
  //
}

//  Member functions
template <typename T>
T Vec3<T>::length() const {
  return std::sqrt(length_squared());
}

template <typename T>
T Vec3<T>::length_squared() const {
  return x * x + y * y + z * z;
}

template <typename T>
void Vec3<T>::normalize() {
  auto inv_len = T(1) / length();
  x *= inv_len;
  y *= inv_len;
  z *= inv_len;
}

template <typename T>
T& Vec3<T>::operator[](int idx) {
  switch (idx) {
    case 0:
      return x;
    case 1:
      return y;
    case 2:
      return z;
    default:
      assert(false);
      return x;
  }
}

template <typename T>
const T& Vec3<T>::operator[](int idx) const {
  switch (idx) {
    case 0:
      return x;
    case 1:
      return y;
    case 2:
      return z;
    default:
      assert(false);
      return x;
  }
}

//  Member ops
template <typename T>
Vec3<T>& Vec3<T>::operator+=(T v) {
  x += v;
  y += v;
  z += v;
  return *this;
}

template <typename T>
Vec3<T>& Vec3<T>::operator-=(T v) {
  x -= v;
  y -= v;
  z -= v;
  return *this;
}

template <typename T>
Vec3<T>& Vec3<T>::operator*=(T v) {
  x *= v;
  y *= v;
  z *= v;
  return *this;
}

template <typename T>
Vec3<T>& Vec3<T>::operator/=(T v) {
  x /= v;
  y /= v;
  z /= v;
  return *this;
}

//  Vec other
template <typename T>
Vec3<T>& Vec3<T>::operator+=(const Vec3<T>& v) {
  x += v.x;
  y += v.y;
  z += v.z;
  return *this;
}

template <typename T>
Vec3<T>& Vec3<T>::operator-=(const Vec3<T>& v) {
  x -= v.x;
  y -= v.y;
  z -= v.z;
  return *this;
}

template <typename T>
Vec3<T>& Vec3<T>::operator*=(const Vec3<T>& v) {
  x *= v.x;
  y *= v.y;
  z *= v.z;
  return *this;
}

template <typename T>
Vec3<T>& Vec3<T>::operator/=(const Vec3<T>& v) {
  x /= v.x;
  y /= v.y;
  z /= v.z;
  return *this;
}

//  (vec, vec)
template <typename T>
inline Vec3<T> operator+(const Vec3<T>& a, const Vec3<T>& b) {
  return Vec3<T>(a.x + b.x, a.y + b.y, a.z + b.z);
}

template <typename T>
inline Vec3<T> operator-(const Vec3<T>& a, const Vec3<T>& b) {
  return Vec3<T>(a.x - b.x, a.y - b.y, a.z - b.z);
}

template <typename T>
inline Vec3<T> operator*(const Vec3<T>& a, const Vec3<T>& b) {
  return Vec3<T>(a.x * b.x, a.y * b.y, a.z * b.z);
}

template <typename T>
inline Vec3<T> operator/(const Vec3<T>& a, const Vec3<T>& b) {
  return Vec3<T>(a.x / b.x, a.y / b.y, a.z / b.z);
}

template <typename T>
inline Vec3<T> operator-(const Vec3<T>& a) {
  return Vec3<T>(-a.x, -a.y, -a.z);
}

//  (T, vec)
template <typename T>
inline Vec3<T> operator+(T v, const Vec3<T>& a) {
  return Vec3<T>(v + a.x, v + a.y, v + a.z);
}

template <typename T>
inline Vec3<T> operator-(T v, const Vec3<T>& a) {
  return Vec3<T>(v - a.x, v - a.y, v - a.z);
}

template <typename T>
inline Vec3<T> operator*(T v, const Vec3<T>& a) {
  return Vec3<T>(v * a.x, v * a.y, v * a.z);
}

template <typename T>
inline Vec3<T> operator/(T v, const Vec3<T>& a) {
  return Vec3<T>(v / a.x, v / a.y, v / a.z);
}

//  (vec, T)
template <typename T>
inline Vec3<T> operator+(const Vec3<T>& a, T v) {
  return Vec3<T>(a.x + v, a.y + v, a.z + v);
}

template <typename T>
inline Vec3<T> operator-(const Vec3<T>& a, T v) {
  return Vec3<T>(a.x - v, a.y - v, a.z - v);
}

template <typename T>
inline Vec3<T> operator*(const Vec3<T>& a, T v) {
  return Vec3<T>(a.x * v, a.y * v, a.z * v);
}

template <typename T>
inline Vec3<T> operator/(const Vec3<T>& a, T v) {
  return Vec3<T>(a.x / v, a.y / v, a.z / v);
}

template <typename T>
inline bool operator==(const Vec3<T>& a, const Vec3<T>& b) {
  return a.x == b.x && a.y == b.y && a.z == b.z;
}

template <typename T>
inline bool operator!=(const Vec3<T>& a, const Vec3<T>& b) {
  return !(a == b);
}

//  Util
template <typename T>
Vec3<T> normalize(const Vec3<T>& a) {
  auto inv_len = T(1) / a.length();
  return Vec3<T>(a.x * inv_len, a.y * inv_len, a.z * inv_len);
}

template <typename T>
Vec3<T> normalize_or_default(const Vec3<T>& a, const Vec3<T>& dflt) {
  auto len = a.length();
  if (len == T(0)) {
    return dflt;
  } else {
    auto inv_len = T(1) / len;
    return Vec3<T>(a.x * inv_len, a.y * inv_len, a.z * inv_len);
  }
}

template <typename T>
T dot(const Vec3<T>& a, const Vec3<T>& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

template <typename T>
Vec3<T> cross(const Vec3<T>& a, const Vec3<T>& b) {
  return Vec3<T>(a.y * b.z - a.z * b.y,
                 a.z * b.x - a.x * b.z,
                 a.x * b.y - a.y * b.x);
}

template <typename T>
Vec3<T> min(const Vec3<T>& a, const Vec3<T>& b) {
  return Vec3<T>(std::min(a.x, b.x),
                 std::min(a.y, b.y),
                 std::min(a.z, b.z));
}

template <typename T>
Vec3<T> max(const Vec3<T>& a, const Vec3<T>& b) {
  return Vec3<T>(std::max(a.x, b.x),
                 std::max(a.y, b.y),
                 std::max(a.z, b.z));
}

template <typename T>
Vec3<T> clamp_each(const Vec3<T>& a, const Vec3<T>& min, const Vec3<T>& max) {
  return Vec3<T>(a.x < min.x ? min.x : a.x > max.x ? max.x : a.x,
                 a.y < min.y ? min.y : a.y > max.y ? max.y : a.y,
                 a.z < min.z ? min.z : a.z > max.z ? max.z : a.z);
}

template <typename T>
Vec3<T> abs(const Vec3<T>& a) {
  return Vec3<T>(std::abs(a.x), std::abs(a.y), std::abs(a.z));
}

template <typename T>
Vec3<T> floor(const Vec3<T>& a) {
  return Vec3<T>(std::floor(a.x), std::floor(a.y), std::floor(a.z));
}

template <typename T>
Vec3<T> ceil(const Vec3<T>& a) {
  return Vec3<T>(std::ceil(a.x), std::ceil(a.y), std::ceil(a.z));
}

template <typename T>
Vec3<T> round(const Vec3<T>& a) {
  return Vec3<T>(std::round(a.x), std::round(a.y), std::round(a.z));
}

template <typename T>
T prod(const Vec3<T>& a) {
  return a.x * a.y * a.z;
}

template <typename T>
T sum(const Vec3<T>& a) {
  return a.x + a.y + a.z;
}

template <typename T>
int max_dimension(const Vec3<T>& a) {
  auto m = a.x;
  int i = 0;

  if (a.y > m) {
    m = a.y;
    i = 1;
  }

  if (a.z > m) {
    i = 2;
  }

  return i;
}

template <typename T>
int min_dimension(const Vec3<T>& a) {
  auto m = a.x;
  int i = 0;

  if (a.y < m) {
    m = a.y;
    i = 1;
  }

  if (a.z < m) {
    i = 2;
  }

  return i;
}

template <typename T>
bool any(const Vec3<T>& a) {
  return bool(a.x) || bool(a.y) || bool(a.z);
}

template <typename T>
bool all(const Vec3<T>& a) {
  return bool(a.x) && bool(a.y) && bool(a.z);
}

template <typename T>
Vec3<bool> lt(const Vec3<T>& a, const Vec3<T>& b) {
  return Vec3<bool>{a.x < b.x, a.y < b.y, a.z < b.z};
}

template <typename T>
Vec3<bool> le(const Vec3<T>& a, const Vec3<T>& b) {
  return Vec3<bool>{a.x <= b.x, a.y <= b.y, a.z <= b.z};
}

template <typename T>
Vec3<bool> gt(const Vec3<T>& a, const Vec3<T>& b) {
  return Vec3<bool>{a.x > b.x, a.y > b.y, a.z > b.z};
}

template <typename T>
Vec3<bool> ge(const Vec3<T>& a, const Vec3<T>& b) {
  return Vec3<bool>{a.x >= b.x, a.y >= b.y, a.z >= b.z};
}

template <typename T>
Vec3<bool> eq(const Vec3<T>& a, const Vec3<T>& b) {
  return Vec3<bool>{a.x == b.x, a.y == b.y, a.z == b.z};
}

template <typename T>
Vec3<bool> ne(const Vec3<T>& a, const Vec3<T>& b) {
  return Vec3<bool>{a.x != b.x, a.y != b.y, a.z != b.z};
}

template <typename T>
Vec3<T> permute(const Vec3<T>& a, int i, int j, int k) {
  return Vec3<T>(a[i], a[j], a[k]);
}

template <typename T>
T det(const Vec3<T>& p0, const Vec3<T>& p1, const Vec3<T>& p2) {
  T m0 = p0.x * (p1.y * p2.z - p2.y * p1.z);
  T m1 = p1.x * (p0.y * p2.z - p2.y * p0.z);
  T m2 = p2.x * (p0.y * p1.z - p1.y * p0.z);
  return m0 - m1 + m2;
}

template <typename T>
Vec2<T> exclude(const Vec3<T>& a, int dim) {
  assert(dim >= 0 && dim < 3);
  return dim == 0 ? Vec2<T>{a.y, a.z} : dim == 1 ? Vec2<T>{a.x, a.z} : Vec2<T>{a.x, a.y};
}

template <typename T>
void invert_implicit_3x3(const Vec3<T>& a, const Vec3<T>& b, const Vec3<T>& c,
                         Vec3<T>* i, Vec3<T>* j, Vec3<T>* k) {
  //  Lengyel, E. Foundations of Game Engine Development, Vol. 1. pp. 48.
  auto r0 = cross(b, c);
  auto r1 = cross(c, a);
  auto r2 = cross(a, b);
  auto inv_det = T(1) / dot(r2, c);
  r0 *= inv_det;
  r1 *= inv_det;
  r2 *= inv_det;
  *i = Vec3<T>{r0.x, r1.x, r2.x};
  *j = Vec3<T>{r0.y, r1.y, r2.y};
  *k = Vec3<T>{r0.z, r1.z, r2.z};
}

template <typename T>
void mul_implicit_3x3(const Vec3<T>& a, const Vec3<T>& b, const Vec3<T>& c,
                      const Vec3<T>& d, const Vec3<T>& e, const Vec3<T>& f,
                      Vec3<T>* i, Vec3<T>* j, Vec3<T>* k) {
  *i = a * d.x + b * d.y + c * d.z;
  *j = a * e.x + b * e.y + c * e.z;
  *k = a * f.x + b * f.y + c * f.z;
}

template <typename T>
Vec3<float> to_vec3f(const Vec3<T>& a) {
  return Vec3<float>{float(a.x), float(a.y), float(a.z)};
}

template <typename T>
Vec3<int> to_vec3i(const Vec3<T>& a) {
  return Vec3<int>{int(a.x), int(a.y), int(a.z)};
}

//  Global typedefs
using Vec3f = Vec3<float>;
using ConstVec3f = ConstVec3<float>;

}