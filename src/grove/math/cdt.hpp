#pragma once

#include "grove/math/Vec2.hpp"
#include <cstdint>
#include <vector>
#include <unordered_set>
#include <functional>

namespace grove::cdt {

/*
 * [1] Anglada, M. V. (1997). An improved incremental algorithm for constructing restricted Delaunay
 *     triangulations. Computers & Graphics, 21(2), 215-223.
 * [2] https://github.com/artem-ogre/CDT
 * [3] https://github.com/wlenthe/GeometricPredicates
 * [4] https://www.cs.cmu.edu/~quake/robust.html
 */

enum class TriPtLoc {
  Outside,
  Inside,
  Edge0,
  Edge1,
  Edge2,
};

using Point = Vec2<double>;

template <typename T>
struct HashPoint {
  std::size_t operator()(const Vec2<T>& p) const noexcept {
    return std::hash<T>{}(p.x) ^ std::hash<T>{}(p.y);
  }
};

struct Triangle {
  uint32_t i[3];

  friend inline bool operator==(const Triangle& a, const Triangle& b) {
    return a.i[0] == b.i[0] && a.i[1] == b.i[1] && a.i[2] == b.i[2];
  }
  friend inline bool operator!=(const Triangle& a, const Triangle& b) {
    return !(a == b);
  }
};

struct Edge {
  struct Hash {
    std::size_t operator()(const Edge& a) const noexcept {
      return std::hash<uint64_t>{}((uint64_t(a.ai) << 32u) | uint64_t(a.bi));
    }
  };
  struct EqualOrderIndependent {
    bool operator()(const Edge& a, const Edge& b) const noexcept {
      return (a.ai == b.ai && a.bi == b.bi) || (a.ai == b.bi && a.bi == b.ai);
    }
  };

  uint32_t ai;
  uint32_t bi;
};

constexpr uint32_t invalid_index() {
  return ~0u;
}

struct Triangulation {
  std::vector<Triangle> triangles;
  std::vector<Point> points;
  std::unordered_set<Edge, Edge::Hash, Edge::EqualOrderIndependent> fixed_edges;
};

void initialize_super_triangle(Triangulation& tri, const Point* points, uint32_t num_points);
void remove_super_triangle(Triangulation& tri);
void add_point(Triangulation& tri, const Point& point);
void add_points(Triangulation& tri, const Point* points, uint32_t num_points);
[[nodiscard]] std::vector<Edge> add_edge(Triangulation& tri, Edge edge);
[[nodiscard]] std::vector<Edge> add_edges(Triangulation& tri, const Edge* edges, uint32_t num_edges);
void validate(const Triangulation& tri);

int hyperplane_side(const Point& line_a, const Point& line_b, const Point& p);
TriPtLoc triangle_point_location(const Point& v0, const Point& v1, const Point& v2, const Point& a);

uint32_t point_index_not_in_edges(const Edge* edges, uint32_t num_edges, uint32_t num_points);
std::vector<uint32_t> find_excluding_hole(const Triangle* tris, uint32_t num_tris,
                                          const Edge* edges, uint32_t num_edges, uint32_t pvi);
void keep_excluding_hole(const Triangle* tris, const uint32_t* keepi, uint32_t num_keep, Triangle* out);
std::vector<Triangle> remove_hole(const Triangle* tris, uint32_t num_tris,
                                  const Edge* edges, uint32_t num_edges, uint32_t num_points);

std::vector<Triangle> triangulate_simple(const Point* points, uint32_t num_points);
std::vector<Triangle> triangulate_simple(const std::vector<Point>& points);
std::vector<Triangle> triangulate_remove_holes_simple(const Point* points, uint32_t num_points,
                                                      const Edge* edges, uint32_t num_edges);

std::vector<uint16_t> to_uint16_indices(const std::vector<Triangle>& tris);

inline const uint32_t* unsafe_cast_to_uint32(const Triangle* tri) {
  return reinterpret_cast<const uint32_t*>(tri);
}

inline uint32_t* unsafe_cast_to_uint32(Triangle* tri) {
  return reinterpret_cast<uint32_t*>(tri);
}

}