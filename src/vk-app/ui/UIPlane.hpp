#pragma once

#include "grove/math/vector.hpp"
#include "grove/math/Bounds3.hpp"

namespace grove {

struct Ray;

class UIPlane {
public:
  struct HitInfo {
    bool hit{};
    Vec2f frac_hit_point{};
  };

public:
  void update(const Ray& mouse_ray, const Vec4f& plane, const Bounds3f& world_bound);
  HitInfo get_mouse_hit_info() const {
    return mouse_hit_info;
  }

private:
  HitInfo mouse_hit_info;
};

}