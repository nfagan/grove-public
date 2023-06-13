#pragma once

#include "Vec2.hpp"
#include <limits>

namespace grove {

template <typename T>
struct Bounds2 {
public:
  using Vector = Vec2<T>;

public:
  constexpr Bounds2();
  constexpr Bounds2(const Vector& min, const Vector& max);

  Vector center() const;
  Vector size() const;
  Vector to_fraction(const Vector& p) const;

  static Bounds2<T> from_min_max_components(const Vector& a, const Vector& b);
  static Bounds2<T> largest();

public:
  Vector min;
  Vector max;
};

template <typename T>
constexpr Bounds2<T>::Bounds2() :
  min{std::numeric_limits<T>::max()},
  max{std::numeric_limits<T>::lowest()} {
  //
}

template <typename T>
constexpr Bounds2<T>::Bounds2(const Vector& min, const Vector& max) :
  min{min}, max{max} {
  //
}

template <typename T>
Bounds2<T> Bounds2<T>::from_min_max_components(const Vector& a, const Vector& b) {
  Vector p0{std::min(a.x, b.x),
            std::min(a.y, b.y)};
  Vector p1{std::max(a.x, b.x),
            std::max(a.y, b.y)};
  return Bounds2<T>{p0, p1};
}

template <typename T>
Bounds2<T> Bounds2<T>::largest() {
  return Bounds2<T>{Vec2<T>(std::numeric_limits<T>::lowest()),
                    Vec2<T>(std::numeric_limits<T>::max())};
}

template <typename T>
inline typename Bounds2<T>::Vector Bounds2<T>::center() const {
  return min + (max - min) / T(2);
}

template <typename T>
inline typename Bounds2<T>::Vector Bounds2<T>::size() const {
  return max - min;
}

template <typename T>
inline typename Bounds2<T>::Vector Bounds2<T>::to_fraction(const Vector& p) const {
  auto span = max - min;
  return (p - min) / span;
}

template <typename T>
Bounds2<T> scale(Bounds2<T> b, const Vec2<T>& frac) {
  auto curr = b.size();
  auto next = curr * frac;
  auto delta = (next - curr) * T(0.5);
  b.min -= delta;
  b.max += delta;
  return b;
}

template <typename T>
Bounds2<T> union_of(const Bounds2<T>& a, const Bounds2<T>& b) {
  return Bounds2<T>{min(a.min, b.min), max(a.max, b.max)};
}

template <typename T>
Bounds2<T> intersect_of(const Bounds2<T>& a, const Bounds2<T>& b) {
  return Bounds2<T>{max(a.min, b.min), min(a.max, b.max)};
}

using Bounds2f = Bounds2<float>;

}