#pragma once

#include "Vec3.hpp"
#include <limits>

namespace grove {

template <typename T>
struct Bounds3 {
public:
  using Vector = Vec3<T>;

public:
  constexpr Bounds3();
  constexpr Bounds3(const Vector& min, const Vector& max);

  Vector center() const;
  Vector size() const;
  Vector to_fraction(const Vector& p) const;

  static Bounds3<T> from_min_max_components(const Vector& a, const Vector& b);
  static Bounds3<T> largest();

  friend inline bool operator==(const Bounds3<T>& a, const Bounds3<T>& b) {
    return a.min == b.min && a.max == b.max;
  }
  friend inline bool operator!=(const Bounds3<T>& a, const Bounds3<T>& b) {
    return !(a == b);
  }

public:
  Vector min;
  Vector max;
};

template <typename T>
constexpr Bounds3<T>::Bounds3() :
  min{std::numeric_limits<T>::max()},
  max{std::numeric_limits<T>::lowest()} {
  //
}

template <typename T>
constexpr Bounds3<T>::Bounds3(const Vector& min, const Vector& max) :
  min{min}, max{max} {
    //
}

template <typename T>
Bounds3<T> Bounds3<T>::from_min_max_components(const Vector& a, const Vector& b) {
  Vector p0{std::min(a.x, b.x),
            std::min(a.y, b.y),
            std::min(a.z, b.z)};
  Vector p1{std::max(a.x, b.x),
            std::max(a.y, b.y),
            std::max(a.z, b.z)};
  return Bounds3<T>(p0, p1);
}

template <typename T>
Bounds3<T> Bounds3<T>::largest() {
  return Bounds3<T>{Vec3<T>(std::numeric_limits<T>::lowest()),
                    Vec3<T>(std::numeric_limits<T>::max())};
}

template <typename T>
inline typename Bounds3<T>::Vector Bounds3<T>::center() const {
  return min + (max - min) / T(2);
}

template <typename T>
inline typename Bounds3<T>::Vector Bounds3<T>::size() const {
  return max - min;
}

template <typename T>
inline typename Bounds3<T>::Vector Bounds3<T>::to_fraction(const Vector& p) const {
  auto span = max - min;
  return (p - min) / span;
}

using Bounds3f = Bounds3<float>;

template <typename T>
void gather_vertices(const Bounds3<T>& aabb, Vec3<T>* out) {
  auto& p0 = aabb.min;
  auto& p1 = aabb.max;
  int i{};
  out[i++] = Vec3<T>{p0.x, p0.y, p0.z};
  out[i++] = Vec3<T>{p1.x, p0.y, p0.z};
  out[i++] = Vec3<T>{p1.x, p1.y, p0.z};
  out[i++] = Vec3<T>{p0.x, p1.y, p0.z};
  //
  out[i++] = Vec3<T>{p0.x, p0.y, p1.z};
  out[i++] = Vec3<T>{p1.x, p0.y, p1.z};
  out[i++] = Vec3<T>{p1.x, p1.y, p1.z};
  out[i++] = Vec3<T>{p0.x, p1.y, p1.z};
}

template <typename T>
void union_of(const Vec3<T>* ps, int num_ps, Vec3<T>* dst_min, Vec3<T>* dst_max) {
  Vec3<T> mn{std::numeric_limits<T>::max()};
  Vec3<T> mx{std::numeric_limits<T>::lowest()};
  for (int i = 0; i < num_ps; i++) {
    mn = min(mn, ps[i]);
    mx = max(mx, ps[i]);
  }
  *dst_min = mn;
  *dst_max = mx;
}

template <typename T>
Bounds3<T> union_of(const Bounds3<T>& a, const Bounds3<T>& b) {
  return Bounds3<T>{min(a.min, b.min), max(a.max, b.max)};
}

template <typename T>
Bounds3<T> intersect_of(const Bounds3<T>& a, const Bounds3<T>& b) {
  return Bounds3<T>{max(a.min, b.min), min(a.max, b.max)};
}

}