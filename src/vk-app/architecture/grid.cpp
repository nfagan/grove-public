#include "grid.hpp"
#include "grove/common/common.hpp"
#include "grove/math/util.hpp"
#include "grove/math/triangle.hpp"
#include "grove/math/constants.hpp"
#include "grove/math/random.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace grid;

struct Edge {
  uint32_t ai;
  uint32_t bi;
};

struct RemovableEdge {
  uint32_t opp_ti;
  Edge opp_edge;
  uint32_t opp_vi;
  uint32_t src_vi;
  double edge_length;
};

constexpr uint32_t no_adjacent_triangle() {
  return ~0u;
}
[[maybe_unused]] bool is_ccw(const Point& p0, const Point& p1, const Point& p2) {
  return det3_implicit(p0, p1, p2) > 0.0;
}
int ccw_triangle(int i) {
  return (i + 1) % 3;
}
int ccw_quad(int i) {
  return (i + 1) % 4;
}
int ccw(const Quad& quad, int i) {
  return quad.is_triangle() ? ccw_triangle(i) : ccw_quad(i);
}
int fibonacci(int n) {
  return n == 0 ? 0 : n == 1 ? 1 : fibonacci(n - 1) + fibonacci(n - 2);
}
bool equal_order_independent(const Edge& a, const Edge& b) {
  return (a.ai == b.ai && a.bi == b.bi) || (a.bi == b.ai && a.ai == b.bi);
}

Quad make_quad(uint32_t ia, uint32_t ib, uint32_t ic, uint32_t id) {
  return {ia, ib, ic, id};
}

Quad make_triangle(uint32_t ia, uint32_t ib, uint32_t ic) {
  Quad quad;
  quad.i[0] = ia;
  quad.i[1] = ib;
  quad.i[2] = ic;
  quad.set_triangle();
  return quad;
}

bool has_edge(const uint32_t* tri, const Edge& e) {
  int ct{};
  for (int i = 0; i < 3; i++) {
    if (tri[i] == e.ai || tri[i] == e.bi) {
      ct++;
    }
  }
  return ct == 2;
}

uint32_t setdiff_edge(const uint32_t* tri, const Edge& e) {
  for (int i = 0; i < 3; i++) {
    if (tri[i] != e.ai && tri[i] != e.bi) {
      return tri[i];
    }
  }
  assert(false);
  return ~0u;
}

int find_point(const Quad& quad, uint32_t vi) {
  for (int i = 0; i < quad.size(); i++) {
    if (quad.i[i] == vi) {
      return i;
    }
  }
  return -1;
}

int find_point(const uint32_t* tri, uint32_t vi) {
  for (int i = 0; i < 3; i++) {
    if (tri[i] == vi) {
      return i;
    }
  }
  return -1;
}

Edge setdiff_point(const uint32_t* tri, uint32_t vi) {
  for (int i = 0; i < 3; i++) {
    if (tri[i] == vi) {
      auto ni = ccw_triangle(i);
      return Edge{tri[ni], tri[ccw_triangle(ni)]};
    }
  }
  assert(false);
  return {~0u, ~0u};
}

uint32_t adjacent_triangle(const uint32_t* tris, uint32_t num_tris, const Edge& edge, uint32_t ti) {
  for (uint32_t i = 0; i < num_tris; i++) {
    if (i != ti) {
      if (has_edge(tris + i * 3, edge)) {
        return i;
      }
    }
  }
  return no_adjacent_triangle();
}

uint32_t require_point(std::vector<Point>& ps, const Point& p, double eps) {
  uint32_t i = 0;
  const auto s = uint32_t(ps.size());
  while (i < s) {
    auto diff = abs(ps[i] - p);
    if (diff[0] < eps && diff[1] < eps) {
      return i;
    }
    i++;
  }
  ps.push_back(p);
  return i;
}

void require_index(FixedPoints& is, uint32_t ind) {
  is.insert(ind);
}

bool is_member(uint32_t i, const FixedPoints& is) {
  return is.count(i) > 0;
}

bool is_preserved_edge(const std::vector<Edge>& edges, const Edge& edge) {
  for (auto& e : edges) {
    if (equal_order_independent(e, edge)) {
      return true;
    }
  }
  return false;
}

void require_preserved_edge(std::vector<Edge>& edges, const Edge& e) {
  if (!is_preserved_edge(edges, e)) {
    edges.push_back(e);
  }
}

void find_removable_edges(const uint32_t* tris, uint32_t num_tris, const Point* ps,
                          const std::vector<Edge>& preserved_edges,
                          const std::unordered_set<uint32_t>& processed,
                          const uint32_t* tri, uint32_t ti,
                          RemovableEdge* candidates,
                          int* num_candidate_edges) {
  for (int i = 0; i < 3; i++) {
    uint32_t vi = tri[i];
    auto opp_edge = setdiff_point(tri, vi);
    if (is_preserved_edge(preserved_edges, opp_edge)) {
      continue;
    }
    uint32_t opp_ti = adjacent_triangle(tris, num_tris, opp_edge, ti);
    if (opp_ti == no_adjacent_triangle() || processed.count(opp_ti)) {
      continue;
    }

    const auto opp_tri = tris + opp_ti * 3;
    const uint32_t opp_vi = setdiff_edge(opp_tri, opp_edge);

    auto& src_p = ps[vi];
    auto& opp_p = ps[opp_vi];
    auto to_src = normalize(opp_p - src_p);

    auto& src_ccw_p = ps[tri[ccw_triangle(i)]];
    auto to_ccw = normalize(src_ccw_p - src_p);

    uint32_t opp_ccw_i = opp_tri[ccw_triangle(find_point(opp_tri, opp_vi))];
    auto& ccw_opp_p = ps[opp_ccw_i];
    auto src_to_ccw_opp = normalize(ccw_opp_p - src_p);
    auto ccw_opp_to_opp = normalize(opp_p - ccw_opp_p);

    const double det_ccw_src = det3_implicit(src_p, src_ccw_p, opp_p);
    const double det_ccw_opp = det3_implicit(opp_p, ccw_opp_p, src_p);

    bool accept =
      1.0 - std::min(1.0, std::abs(dot(to_ccw, to_src))) > 1e-2 &&
      1.0 - std::min(1.0, std::abs(dot(src_to_ccw_opp, ccw_opp_to_opp))) > 1e-2;
    if (accept) {
#if 1
      assert(det_ccw_src > 0.0 && det_ccw_opp > 0.0);
      (void) det_ccw_src;
      (void) det_ccw_opp;
#else
      accept = det_ccw_src > 0.0 && det_ccw_opp > 0.0;
#endif
    }
    if (accept) {
      RemovableEdge candidate{};
      candidate.opp_ti = opp_ti;
      candidate.opp_edge = opp_edge;
      candidate.opp_vi = opp_vi;
      candidate.src_vi = vi;
      candidate.edge_length = (ps[opp_edge.bi] - ps[opp_edge.ai]).length();
      candidates[(*num_candidate_edges)++] = candidate;
    }
  }
}

Quad try_convert_to_quad(const uint32_t* tris, uint32_t num_tris, const Point* ps,
                         std::vector<Edge>& preserved_edges,
                         std::unordered_set<uint32_t>& processed,
                         const uint32_t* tri, uint32_t ti, const PermitQuad& permit_quad) {
  RemovableEdge candidate_edges[3];
  int num_candidates{};
  find_removable_edges(
    tris, num_tris, ps, preserved_edges, processed, tri, ti, candidate_edges, &num_candidates);

  processed.insert(ti);
  if (num_candidates > 0 && permit_quad()) {
    const RemovableEdge* best_edge = std::max_element(
      candidate_edges,
      candidate_edges + num_candidates,
      [](const auto& a, const auto& b) {
        return a.edge_length < b.edge_length;
      });
    processed.insert(best_edge->opp_ti);

    const uint32_t src_vi = best_edge->src_vi;
    const uint32_t opp_vi = best_edge->opp_vi;
    const Edge opp_edge = best_edge->opp_edge;

    require_preserved_edge(preserved_edges, Edge{src_vi, opp_edge.ai});
    require_preserved_edge(preserved_edges, Edge{src_vi, opp_edge.bi});
    require_preserved_edge(preserved_edges, Edge{opp_edge.bi, opp_vi});
    require_preserved_edge(preserved_edges, Edge{opp_vi, opp_edge.ai});

    int src_i = find_point(tri, src_vi);
    int next_i0 = ccw_triangle(src_i);
    int next_i1 = ccw_triangle(next_i0);
    Quad quad{src_vi, tri[next_i0], opp_vi, tri[next_i1]};
    return quad;
  } else {
    //  Reject quad or unable to split triangles, so keep triangle.
    for (int i = 0; i < 3; i++) {
      require_preserved_edge(preserved_edges, Edge{tri[i], tri[ccw_triangle(i)]});
    }
    auto t = make_triangle(tri[0], tri[1], tri[2]);
    return t;
  }
}

[[maybe_unused]] void check_ccw(const Quad* quads, uint32_t num_quads, const Point* ps, double eps) {
  (void) eps;
  for (uint32_t i = 0; i < num_quads; i++) {
    auto& q = quads[i];
    for (int j = 0; j < q.size(); j++) {
      for (int k = 0; k < q.size(); k++) {
        if (j != k) {
          auto d = ps[q.i[j]] - ps[q.i[k]];
          assert(d.length() > eps);
          (void) d;
        }
      }
    }
    assert(is_ccw(ps[q.i[0]], ps[q.i[1]], ps[q.i[2]]));
  }
}

Point centroid(const Quad& q, const Point* ps) {
  Point cent{};
  for (int i = 0; i < q.size(); i++) {
    cent += ps[q.i[i]];
  }
  cent /= double(q.size());
  return cent;
}

void relax_neighbors(const Point* ps, uint32_t num_points,
                     const Quad* quads, uint32_t num_quads,
                     const FixedPoints& fixed_pi, const RelaxParams& params, Point* forces) {
  for (uint32_t pi = 0; pi < num_points; pi++) {
    for (uint32_t qi = 0; qi < num_quads; qi++) {
      auto& q = quads[qi];
      int ip = find_point(q, pi);
      if (ip == -1) {
        continue;
      }
      auto next_ip = ccw(q, ip);
      uint32_t i0 = q.i[ip];
      uint32_t i1 = q.i[next_ip];
      auto curr_v = ps[i0] - ps[next_ip];
      auto curr_dist = curr_v.length();
      curr_v /= curr_dist;
      auto to_curr = curr_dist - params.target_neighbor_length;
      auto f = to_curr * params.neighbor_length_scale * curr_v;
      if (!is_member(i0, fixed_pi)) {
        forces[i0] -= f * (1.0 + (urand() * 2.0 - 1.0) * params.neighbor_random_scale);
      }
      if (!is_member(i1, fixed_pi)) {
        forces[i1] += f * (1.0 + (urand() * 2.0 - 1.0) * params.neighbor_random_scale);
      }
    }
  }
}

void relax_quads(const Point* ps, const Quad* quads, uint32_t num_quads,
                 const FixedPoints& fixed_pi, const RelaxParams& params, Point* forces) {
  for (uint32_t qi = 0; qi < num_quads; qi++) {
    auto& q = quads[qi];
    if (q.is_triangle()) {
      continue;
    }
    auto cent = centroid(q, ps);
    for (int i = 0; i < 4; i++) {
      auto e0i0 = i;
      auto e0i1 = ccw_quad(i);
      auto e1i0 = e0i1;
      auto e1i1 = ccw_quad(e0i1);

      auto e0 = ps[q.i[e0i1]] - ps[q.i[e0i0]];
      auto e1 = ps[q.i[e1i1]] - ps[q.i[e1i0]];
      auto n = Point{-e1.y, e1.x};
      auto fn = dot(n, e0);
#if 1
      int qi_cent[] = {e1i1};
      const int num_qi_cent = 1;
#else
      int qi_cent[] = {e0i0, e0i1, e1i1};
      const int num_qi_cent = 3;
#endif
      for (int j = 0; j < num_qi_cent; j++) {
        int qic = qi_cent[j];
        uint32_t pi = q.i[qic];
        if (!is_member(pi, fixed_pi)) {
          auto to_cent = cent - ps[pi];
          forces[pi] -= to_cent * fn * double(params.quad_scale);
        }
      }
    }
  }
}

} //  anon

std::vector<Quad> grid::subdivide(const std::vector<Quad>& quads, std::vector<Point>& dst_points,
                                  FixedPoints& fixed_pi) {
  return subdivide(quads.data(), uint32_t(quads.size()), dst_points, fixed_pi);
}

std::vector<Quad> grid::subdivide(const Quad* quads, uint32_t num_quads,
                                  std::vector<Point>& dst_points, FixedPoints& fixed_pi) {
  const auto edge_center = [](const std::vector<Point>& ps, uint32_t pi0, uint32_t pi1) {
    return (ps[pi1] - ps[pi0]) * 0.5 + ps[pi0];
  };

  const double eps = 1e-5;
  std::vector<Quad> result;
  for (uint32_t qi = 0; qi < num_quads; qi++) {
    auto& q = quads[qi];
    auto cent = centroid(q, dst_points.data());

    const uint32_t pi0 = q.i[0];
    const uint32_t pi1 = q.i[1];
    const uint32_t pi2 = q.i[2];
    const uint32_t pi3 = q.i[3];
    auto i_cent = require_point(dst_points, cent, eps);
    auto i_e0 = require_point(dst_points, edge_center(dst_points, pi0, pi1), eps);
    auto i_e1 = require_point(dst_points, edge_center(dst_points, pi1, pi2), eps);

    uint32_t i_e2;
    uint32_t i_e3{};
    if (q.is_triangle()) {
      i_e2 = require_point(dst_points, edge_center(dst_points, pi2, pi0), eps);
      result.push_back(make_quad(pi0, i_e0, i_cent, i_e2));
      result.push_back(make_quad(i_e0, pi1, i_e1, i_cent));
      result.push_back(make_quad(i_cent, i_e1, pi2, i_e2));
    } else {
      i_e2 = require_point(dst_points, edge_center(dst_points, pi2, pi3), eps);
      i_e3 = require_point(dst_points, edge_center(dst_points, pi3, pi0), eps);
      result.push_back(make_quad(pi0, i_e0, i_cent, i_e3));
      result.push_back(make_quad(i_e0, pi1, i_e1, i_cent));
      result.push_back(make_quad(i_cent, i_e1, pi2, i_e2));
      result.push_back(make_quad(i_e3, i_cent, i_e2, pi3));
    }

    if (is_member(pi0, fixed_pi)) {
      require_index(fixed_pi, i_e0);
    }
    if (is_member(pi1, fixed_pi)) {
      require_index(fixed_pi, i_e1);
    }
    if (is_member(pi2, fixed_pi)) {
      require_index(fixed_pi, i_e2);
    }
    if (!q.is_triangle() && is_member(pi3, fixed_pi)) {
      require_index(fixed_pi, i_e3);
    }
  }

  return result;
}

std::vector<Quad> grid::convert_to_quads(const uint32_t* tris, uint32_t num_tris, const Point* ps,
                                         const PermitQuad& permit_quad) {
  std::vector<Quad> result;
  if (num_tris == 0) {
    return result;
  }

  std::vector<uint32_t> pend;
  pend.push_back(0);

  std::unordered_set<uint32_t> visited;
  visited.insert(0);

  std::unordered_set<uint32_t> processed;
  std::vector<Edge> preserved_edges;
  while (!pend.empty()) {
    auto ti = pend.back();
    pend.pop_back();
    const auto* tri = tris + ti * 3;

    if (!is_member(ti, processed)) {
      result.push_back(try_convert_to_quad(
        tris, num_tris, ps, preserved_edges, processed, tri, ti, permit_quad));
    }

    for (int i = 0; i < 3; i++) {
      Edge edge{tri[i], tri[ccw_triangle(i)]};
      auto adj_ti = adjacent_triangle(tris, num_tris, edge, ti);
      if (adj_ti != no_adjacent_triangle() && visited.count(adj_ti) == 0) {
        pend.push_back(adj_ti);
        visited.insert(adj_ti);
      }
    }
  }
#ifdef GROVE_DEBUG
  check_ccw(result.data(), uint32_t(result.size()), ps, 1e-3);
#endif
  return result;
}

void grid::relax(std::vector<Point>& ps, std::vector<Quad>& quads, const FixedPoints& fixed_pi,
                 const RelaxParams& params) {
  return relax(
    ps.data(), uint32_t(ps.size()), quads.data(), uint32_t(quads.size()), fixed_pi, params);
}

void grid::relax(Point* ps, uint32_t num_points, const Quad* quads, uint32_t num_quads,
                 const FixedPoints& fixed_pi, const RelaxParams& params) {
  std::vector<Point> velocities(num_points);
  std::vector<Point> forces(num_points);

  for (int iter = 0; iter < params.iters; iter++) {
    if (params.neighbor_length_scale > 0.0f) {
      relax_neighbors(ps, num_points, quads, num_quads, fixed_pi, params, forces.data());
    }
    if (params.quad_scale > 0.0f) {
      relax_quads(ps, quads, num_quads, fixed_pi, params, forces.data());
    }

    const double dt = params.dt;
    const double dt2 = dt * dt;
    for (uint32_t i = 0; i < num_points; i++) {
      auto f = forces[i];
      auto& v = velocities[i];
      auto last_p = ps[i];
      ps[i] += v * dt + f * dt2;
      v = ps[i] - last_p;
    }
    std::fill(forces.begin(), forces.end(), Point{});
  }
}

void grid::make_hexagon_points(int fib_n, std::vector<Point>& dst_points, FixedPoints& fixed_pi) {
  const int n = fibonacci(std::max(3, fib_n));
  const Point pl{-1.0, 0.5};
  const Point pr{1.0, 0.5};
  const Point pt{0.0, 1.0};
  const Point pl_bot{-1.0, -0.5};

  std::vector<uint32_t> base_x;
  std::vector<uint32_t> next_base_x;
  const double eps = 1e-5;

  const auto tri_beg = uint32_t(dst_points.size());
  for (int i = 0; i < n; i++) {
    const double edge_t = double(i) / double(n-1);
    const auto p0_l = lerp(edge_t, pl, pt);
    const auto p0_r = lerp(edge_t, pr, pt);

    const auto li = require_point(dst_points, p0_l, eps);
    const auto ri = require_point(dst_points, p0_r, eps);
    require_index(fixed_pi, li);
    require_index(fixed_pi, ri);

    if (i == 0) {
      base_x.push_back(li);
      base_x.push_back(ri);
    } else if (i == 1) {
      next_base_x.push_back(li);
      next_base_x.push_back(ri);
    }

    const int use_n = n - (i + 1);
    if (use_n > 1) {
      for (int j = 0; j < use_n-1; j++) {
        auto p = lerp(double(j + 1) / double(use_n), p0_l, p0_r);
        auto pi = require_point(dst_points, p, eps);
        if (i == 0) {
          base_x.push_back(pi);
        } else if (i == 1) {
          next_base_x.push_back(pi);
        }
      }
    }
  }
  const auto tri_end = uint32_t(dst_points.size());
  for (uint32_t i = tri_beg; i < tri_end; i++) {
    auto p = dst_points[i];
    p.y -= 0.5;
    p.y = -p.y - 0.5;
    auto pi = require_point(dst_points, p, eps);
    if (is_member(i, fixed_pi)) {
      require_index(fixed_pi, pi);
    }
  }

  for (int i = 2; i <= n * 2 - 2; i++) {
    double y = lerp(double(i - 1) / double(n - 1) * 0.5, pl.y, pl_bot.y);
    const auto* ps = i % 2 == 1 ? &base_x : &next_base_x;
    for (uint32_t next_pi : *ps) {
      const Point p{dst_points[next_pi].x, y};
      auto pi = require_point(dst_points, p, eps);
      if (std::abs(1.0 - std::abs(p.x)) < eps) {
        require_index(fixed_pi, pi);
      }
    }
  }
}

GROVE_NAMESPACE_END
