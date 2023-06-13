#include "triangulation.hpp"
#include "grove/common/common.hpp"
#include "grove/common/vector_util.hpp"
#include <unordered_map>

GROVE_NAMESPACE_BEGIN

namespace {

/*

https://en.wikipedia.org/wiki/Bowyer%E2%80%93Watson_algorithm

circumcircle(tri):
  e0 = p1 - p0
  l0 = p2 - dot(p2, e0) * e0

Delaunay(pts):
  T = {}
  super_tri = triangle large enough to contain all pts
  insert(T, super_tri)

  for every point p in pts:
    bad_tris = {}

    for each triangle t in T:
      if p in circumcircle of t:
        insert(bad_tris, t)

    polygon = {}

    for each triangle t in bad_tris:
      for each edge e in t:
        if e not shared with other triangles in bad_tris:
          add e to polygon

    erase(T, bad_tris)

    for each edge e in polygon:
      form a triangle t between e and p
      insert(T, t)

  % Remove triangles formed with vertices of super triangle.
  for each triangle t in T:
    if t contains a vertex of super_tri:
      erase(T, t)
 */

constexpr float super_triangle_extent = 128.0f;

using Points = std::vector<Vec2f>;

struct Circumcircle {
  Vec2f position;
  float radius;
};

using Edge = Vec2<int>;

struct HashEdge {
  std::size_t operator()(const Edge& e) const noexcept {
    return std::hash<int>{}(e.x) ^ std::hash<int>{}(e.y);
  }
};

bool in_circle(const Circumcircle& c, const Vec2f& p) {
  auto l = p - c.position;
  return dot(l, l) <= c.radius * c.radius;
}

tri::Triangle super_triangle_indices(int num_points) {
  return {{num_points, num_points + 1, num_points + 2}};
}

Vec2f ray_ray_intersect(const Vec2f& p0, const Vec2f& d0, const Vec2f& p1, const Vec2f& d1) {
  //  Lengyel, E. Mathematics for 3D Game Programming and Computer Graphics. pp 96.

  auto d = dot(d0, d1);
  float denom = 1.0f / (d * d - (dot(d0, d0) * dot(d1, d1)));

  Vec2f col0{-dot(d1, d1), -dot(d0, d1)};
  Vec2f col1{dot(d0, d1), dot(d0, d0)};
  Vec2f t{dot(p1 - p0, d0), dot(p1 - p0, d1)};

  auto ts = denom * (t.x * col0 + t.y * col1);
  return p0 + d0 * ts.x;
}

Circumcircle circumcircle(const Points& points, const tri::Triangle& triangle) {
  auto& p0 = points[triangle.indices[0]];
  auto& p1 = points[triangle.indices[1]];
  auto& p2 = points[triangle.indices[2]];

  auto e0 = p1 - p0;
  auto e1 = p2 - p0;

  auto proj0 = (p0 + p1) * 0.5f;
  auto dir0 = Vec2f{e0.y, -e0.x};

  auto proj1 = (p0 + p2) * 0.5f;
  auto dir1 = Vec2f{e1.y, -e1.x};

  auto p = ray_ray_intersect(proj0, dir0, proj1, dir1);
  auto p_diff = p0 - p;
  auto r = std::sqrt(dot(p_diff, p_diff));

  return {p, r};
}

void add_super_triangle_points(Points& points) {
  Vec2f p0{-super_triangle_extent, -super_triangle_extent};
  Vec2f p1{super_triangle_extent, -super_triangle_extent};
  Vec2f p2{super_triangle_extent * 0.5f, super_triangle_extent};

  points.push_back(p0);
  points.push_back(p1);
  points.push_back(p2);
}

void sort2(Edge& inds) {
  if (inds.x > inds.y) {
    std::swap(inds.x, inds.y);
  }
}

void triangle_edge_indices(const tri::Triangle& tri, Edge* out) {
  auto e0 = Edge{tri.indices[0], tri.indices[1]};
  auto e1 = Edge{tri.indices[1], tri.indices[2]};
  auto e2 = Edge{tri.indices[2], tri.indices[0]};

  sort2(e0);
  sort2(e1);
  sort2(e2);

  out[0] = e0;
  out[1] = e1;
  out[2] = e2;
}

std::vector<Edge> unique_edges(const std::vector<Edge>& bad_tri_edges) {
  std::unordered_map<Edge, int, HashEdge> visited_edges;
  std::vector<Edge> result;

  for (const auto& edge : bad_tri_edges) {
    auto it = visited_edges.find(edge);
    if (it != visited_edges.end()) {
      it->second++;
    } else {
      visited_edges[edge] = 1;
    }
  }

  for (const auto& it : visited_edges) {
    if (it.second == 1) {
      result.push_back(it.first);
    }
  }

  return result;
}

bool has_super_triangle_vertex(const tri::Triangle& tri, int num_points) {
  for (int i = 0; i < 3; i++) {
    auto check_for = num_points + i;
    for (const auto& ind : tri.indices) {
      if (ind == check_for) {
        return true;
      }
    }
  }

  return false;
}

}

std::vector<tri::Triangle> tri::delaunay_triangulate(Points points) {
  auto orig_num_points = int(points.size());

  std::vector<tri::Triangle> triangles{super_triangle_indices(orig_num_points)};
  add_super_triangle_points(points);

  std::vector<int> bad_tri_indices;
  std::vector<Triangle> bad_tris;
  std::vector<Edge> bad_tri_edges;

  const int actual_num_points = int(points.size()) - 3; //  ignore super triangle end points.
  for (int i = 0; i < actual_num_points; i++) {
    const auto& p = points[i];

    for (int j = 0; j < int(triangles.size()); j++) {
      auto& t = triangles[j];
      auto circ = circumcircle(points, t);

      if (in_circle(circ, p)) {
        bad_tris.push_back(t);
        bad_tri_indices.push_back(j);
      }
    }

    for (auto& t : bad_tris) {
      Edge tmp_edges[3];
      triangle_edge_indices(t, tmp_edges);

      for (auto& edge : tmp_edges) {
        bad_tri_edges.push_back(edge);
      }
    }

    erase_set(triangles, bad_tri_indices);

    auto poly_edges = unique_edges(bad_tri_edges);
    for (auto& edge : poly_edges) {
      tri::Triangle t{{edge.x, edge.y, i}};
      triangles.push_back(t);
    }

    bad_tris.clear();
    bad_tri_indices.clear();
    bad_tri_edges.clear();
  }

  //  Remove triangles with super-triangle vertex.
  for (int i = 0; i < int(triangles.size()); i++) {
    if (has_super_triangle_vertex(triangles[i], orig_num_points)) {
      bad_tri_indices.push_back(i);
    }
  }

  erase_set(triangles, bad_tri_indices);

  return triangles;
}

std::vector<Vec2f> tri::to_2d_xz(const std::vector<Vec3f>& points) {
  std::vector<Vec2f> pos_2d;
  pos_2d.reserve(points.size());
  for (auto& p : points) {
    pos_2d.emplace_back(p.x, p.z);
  }
  return pos_2d;
}

GROVE_NAMESPACE_END

