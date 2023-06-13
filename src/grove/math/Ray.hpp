#pragma once

#include "Vec3.hpp"

namespace grove {

struct Ray {
  Vec3f operator()(float t) const;

  Vec3f origin;
  Vec3f direction;
};

inline Vec3f Ray::operator()(float t) const {
  return origin + direction * t;
}

}
