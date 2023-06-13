#pragma once

#include "grove/math/vector.hpp"
#include "grove/math/OBB3.hpp"
#include <vector>

namespace grove::arch {

struct TriangulatedGrid {
  const Vec2<double>* points;
  uint32_t num_points;
  const uint32_t* tris;
  uint32_t num_tris;
};

template <typename T>
std::vector<Vec2<T>> make_grid(int w, int h) {
  assert(w > 1 && h > 1);
  std::vector<Vec2<T>> res;
  for (int i = 0; i < w; i++) {
    for (int j = 0; j < h; j++) {
      auto x = T(i) / T(w-1);
      auto y = T(j) / T(h-1);
      res.emplace_back(x, y);
    }
  }
  return res;
}

//  center x, *base y*, center z
OBB3f make_obb_xz(const Vec3f& c, float theta, const Vec3f& full_size);
OBB3f extrude_obb_xz(const OBB3f& a, float dth, const Vec3f& full_size);

}