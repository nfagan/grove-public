#pragma once

#include "Vec3.hpp"

namespace grove {

struct CubicBezierCurvePoints {
public:
  CubicBezierCurvePoints() = default;
  CubicBezierCurvePoints(const Vec3f& p0, const Vec3f& p1, const Vec3f& p2, const Vec3f& p3) :
    p0{p0}, p1{p1}, p2{p2}, p3{p3} {
    //
  }

  Vec3f evaluate(float t) const;
  const Vec3f& operator[](int index) const;
  Vec3f& operator[](int index);

public:
  Vec3f p0{};
  Vec3f p1{};
  Vec3f p2{};
  Vec3f p3{};
};

inline const Vec3f& CubicBezierCurvePoints::operator[](int index) const {
  assert(index >= 0 && index < 4);

  switch (index) {
    case 0:
      return p0;
    case 1:
      return p1;
    case 2:
      return p2;
    case 3:
      return p3;
    default:
      assert(false);
      return p0;
  }
}

inline Vec3f& CubicBezierCurvePoints::operator[](int index) {
  assert(index >= 0 && index < 4);

  switch (index) {
    case 0:
      return p0;
    case 1:
      return p1;
    case 2:
      return p2;
    case 3:
      return p3;
    default:
      assert(false);
      return p0;
  }
}

}