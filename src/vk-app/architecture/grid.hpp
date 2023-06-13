#pragma once

#include "grove/math/vector.hpp"
#include <vector>
#include <unordered_set>
#include <functional>

namespace grove::grid {

//  https://www.redblobgames.com/grids/hexagons/
//  https://twitter.com/OskSta/status/1147881669350891521

using Point = Vec2<double>;
using FixedPoints = std::unordered_set<uint32_t>;
using PermitQuad = std::function<bool()>;

struct Quad {
  int size() const {
    return is_triangle() ? 3 : 4;
  }
  void set_triangle() {
    i[3] = ~0u;
  }
  bool is_triangle() const {
    return i[3] == ~0u;
  }
  uint32_t i[4];
};

struct RelaxParams {
  float dt{0.1f};
  int iters{1024};
  float neighbor_length_scale{};
  float quad_scale{1024.0f};
  float target_neighbor_length{0.05f};
  float neighbor_random_scale{};
};

void make_hexagon_points(int fib_n, std::vector<Point>& dst_points, FixedPoints& fixed_pi);
std::vector<Quad> convert_to_quads(const uint32_t* tris, uint32_t num_tris,
                                   const Point* ps, const PermitQuad& permit_quad);
std::vector<Quad> subdivide(const Quad* quads, uint32_t num_quads,
                            std::vector<Point>& dst_points, FixedPoints& fixed_pi);
std::vector<Quad> subdivide(const std::vector<Quad>& quads,
                            std::vector<Point>& dst_points, FixedPoints& fixed_pi);

void relax(Point* ps, uint32_t num_points, const Quad* quads, uint32_t num_quads,
           const FixedPoints& fixed_pi, const RelaxParams& params);
void relax(std::vector<Point>& ps, std::vector<Quad>& quads,
           const FixedPoints& fixed_pi, const RelaxParams& params);

}