#pragma once

#include "vector.hpp"
#include <vector>

namespace grove::tri {

struct Triangle {
  int indices[3]{};
};

//  @Note: taken by value because we add dummy points before computing the triangulation.
std::vector<Triangle> delaunay_triangulate(std::vector<Vec2f> points);

//  Convert 3d points to 2d points, keeping xz coordinates.
std::vector<Vec2f> to_2d_xz(const std::vector<Vec3f>& points);

template <typename T>
std::vector<T> flatten_triangle_indices(const std::vector<Triangle>& tris) {
  std::vector<T> result;
  result.reserve(tris.size() * 3);

  for (auto& tri : tris) {
    for (auto& ind : tri.indices) {
      result.push_back(T(ind));
    }
  }

  return result;
}

}