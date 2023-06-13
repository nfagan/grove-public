#include "cdt.hpp"
#include "grove/common/common.hpp"
#include "grove/common/platform.hpp"
#include "grove/common/logging.hpp"
#ifdef GROVE_MACOS
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-copy"
#endif
#include "GeometricPredicates/predicates.h"
#ifdef GROVE_MACOS
#pragma clang diagnostic pop
#endif
#include <cassert>

GROVE_NAMESPACE_BEGIN

namespace {

using namespace cdt;

template <typename T>
T ccw(T index) {
  assert(index >= 0 && index < 3);
  return (index + T(1)) % T(3);
}

template <typename T>
T cw(T index) {
  assert(index >= 0 && index < 3);
  return index == T(0) ? T(2) : index - T(1);
}

[[maybe_unused]] bool is_duplicate(const std::vector<Point>& points, const Point& p) {
  for (auto& point : points) {
    if (point == p) {
      return true;
    }
  }
  return false;
}

Triangle make_triangle(uint32_t ai, uint32_t bi, uint32_t ci) {
  return Triangle{{ai, bi, ci}};
}
Edge make_edge(uint32_t ai, uint32_t bi) {
  return Edge{ai, bi};
}

int find_point_index(const Triangle& tri, uint32_t pi) {
  for (int i = 0; i < 3; i++) {
    if (tri.i[i] == pi) {
      return i;
    }
  }
  return -1;
}

bool has_point_index(const Triangle& tri, uint32_t pi) {
  return tri.i[0] == pi || tri.i[1] == pi || tri.i[2] == pi;
}

bool has_point_index(const Edge& e, uint32_t pi) {
  return e.ai == pi || e.bi == pi;
}

bool is_on_edge(TriPtLoc loc) {
  return loc == TriPtLoc::Edge0 || loc == TriPtLoc::Edge1 || loc == TriPtLoc::Edge2;
}

bool is_ccw(const Point& a, const Point& b, const Point& c) {
  return det3_implicit(a, b, c) > 0;
}

bool is_ccw(const Triangulation& tri, uint32_t ai, uint32_t bi, uint32_t ci) {
  assert(ai < uint32_t(tri.points.size()) &&
         bi < uint32_t(tri.points.size()) &&
         ci < uint32_t(tri.points.size()));
  auto& p0 = tri.points[ai];
  auto& p1 = tri.points[bi];
  auto& p2 = tri.points[ci];
  return is_ccw(p0, p1, p2);
}

Triangle require_ccw(const Triangulation& tri, uint32_t ai, uint32_t bi, uint32_t ci) {
  if (!is_ccw(tri, ai, bi, ci)) {
    return make_triangle(ai, ci, bi);
  } else {
    return make_triangle(ai, bi, ci);
  }
}

uint32_t num_triangles(const Triangulation& tri) {
  return uint32_t(tri.triangles.size());
}
uint32_t num_points(const Triangulation& tri) {
  return uint32_t(tri.points.size());
}

uint32_t add_triangle(Triangulation& tri, const Triangle& t) {
  assert(is_ccw(tri, t.i[0], t.i[1], t.i[2]));
  auto ti = uint32_t(tri.triangles.size());
  tri.triangles.push_back(t);
  return ti;
}

uint32_t add_point(Triangulation& tri, const Point& p) {
#ifdef GROVE_DEBUG
  assert(!is_duplicate(tri.points, p));
#endif
  assert(std::isfinite(p.x) && std::isfinite(p.y));
  auto pi = num_points(tri);
  tri.points.push_back(p);
  return pi;
}

bool is_fixed_edge(const Triangulation& tri, const Edge& e) {
  return tri.fixed_edges.count(e) > 0;
}

std::tuple<const Point*, const Point*, const Point*>
read_vertices(const Point* points, const Triangle& t) {
  return std::make_tuple(points + t.i[0], points + t.i[1], points + t.i[2]);
}

Edge edge_indices(const Triangle& t, TriPtLoc loc) {
  if (loc == TriPtLoc::Edge0) {
    return make_edge(t.i[0], t.i[1]);
  } else if (loc == TriPtLoc::Edge1) {
    return make_edge(t.i[1], t.i[2]);
  } else {
    assert(loc == TriPtLoc::Edge2);
    return make_edge(t.i[2], t.i[0]);
  }
}

bool has_edge(const Triangle& t, const Edge& edge) {
  bool has_ai{};
  bool has_bi{};
  for (auto& vi : t.i) {
    if (vi == edge.ai) {
      has_ai = true;
    }
    if (vi == edge.bi) {
      has_bi = true;
    }
  }
  return has_ai && has_bi;
}

bool has_edge(const Triangulation& tri, const Edge& e) {
  for (auto& t : tri.triangles) {
    if (has_edge(t, e)) {
      return true;
    }
  }
  return false;
}

uint32_t edge_neighbor(const Triangle* tris, uint32_t num_tris, uint32_t t, const Edge& e) {
  for (uint32_t ti = 0; ti < num_tris; ti++) {
    if (ti != t && has_edge(tris[ti], e)) {
      return ti;
    }
  }
  return invalid_index();
}

uint32_t setdiff_edge(const Triangle& t, const Edge& edge) {
  for (auto& vi : t.i) {
    if (!has_point_index(edge, vi)) {
      return vi;
    }
  }
  assert(false);
  return invalid_index();
}

Edge setdiff_point(const Triangle& t, uint32_t pi) {
  for (uint32_t i = 0; i < 3; i++) {
    auto i0 = i;
    auto i1 = ccw(i);
    if (t.i[i0] != pi && t.i[i1] != pi) {
      return make_edge(t.i[i0], t.i[i1]);
    }
  }
  assert(false);
  return {};
}

bool is_edge(const Edge* edges, uint32_t num_edges, const Edge& edge) {
  for (uint32_t i = 0; i < num_edges; i++) {
    if (Edge::EqualOrderIndependent{}(edges[i], edge)) {
      return true;
    }
  }
  return false;
}

uint32_t adjacent_triangle(const Triangle* tris, uint32_t num_tris, uint32_t ti, const Edge& edge) {
  for (uint32_t i = 0; i < num_tris; i++) {
    if (i != ti && has_edge(tris[i], edge)) {
      return i;
    }
  }
  return invalid_index();
}

std::tuple<uint32_t, Edge> opposed_triangle(const Triangle* tris, uint32_t num_tris,
                                            uint32_t ti, uint32_t pi) {
  auto shared_edge = setdiff_point(tris[ti], pi);
  auto adj_ti = adjacent_triangle(tris, num_tris, ti, shared_edge);
  if (adj_ti == invalid_index()) {
    return std::make_tuple(invalid_index(), Edge{});
  } else {
    return std::make_tuple(adj_ti, shared_edge);
  }
}

std::tuple<uint32_t, uint32_t, Edge>
triangles_containing_point(const Triangulation& tri, const Point& p) {
  for (uint32_t ti = 0; ti < num_triangles(tri); ti++) {
    auto [p0, p1, p2] = read_vertices(tri.points.data(), tri.triangles[ti]);
    auto query_loc = triangle_point_location(*p0, *p1, *p2, p);
    if (is_on_edge(query_loc)) {
      auto shared_edge = edge_indices(tri.triangles[ti], query_loc);
      const uint32_t neighbor_ti = edge_neighbor(
        tri.triangles.data(), uint32_t(tri.triangles.size()), ti, shared_edge);
      assert(neighbor_ti != invalid_index() && neighbor_ti != ti);
      return std::make_tuple(ti, neighbor_ti, shared_edge);
    } else if (query_loc == TriPtLoc::Inside) {
      return std::make_tuple(ti, invalid_index(), Edge{});
    }
  }
  assert(false && "No triangle found.");
  return {};
}

void add_point_in_triangle(Triangulation& tri, uint32_t ti, uint32_t pi, uint32_t* new_ti) {
  auto& t = tri.triangles[ti];
  uint32_t ai = t.i[0];
  uint32_t bi = t.i[1];
  uint32_t ci = t.i[2];
  auto t0 = make_triangle(ai, pi, ci);
  auto t1 = make_triangle(pi, bi, ci);
  auto t2 = make_triangle(ai, bi, pi);
  t = t0;
  new_ti[0] = ti;
  new_ti[1] = add_triangle(tri, t1);
  new_ti[2] = add_triangle(tri, t2);
}

void add_point_on_edge(Triangulation& tri, uint32_t ti0, uint32_t ti1,
                       uint32_t pi, const Edge& shared_edge, uint32_t* new_ti) {
  auto& t0 = tri.triangles[ti0];
  auto& t1 = tri.triangles[ti1];
  assert(has_edge(t0, shared_edge) && has_edge(t1, shared_edge) && ti0 != ti1);
  uint32_t ai = setdiff_edge(t0, shared_edge);
  uint32_t bi = shared_edge.ai;
  uint32_t ci = setdiff_edge(t1, shared_edge);
  uint32_t di = shared_edge.bi;
  new_ti[0] = ti0;
  new_ti[1] = ti1;
  t0 = require_ccw(tri, ai, bi, pi);
  t1 = require_ccw(tri, pi, di, ai);
  new_ti[2] = add_triangle(tri, require_ccw(tri, bi, ci, pi));
  new_ti[3] = add_triangle(tri, require_ccw(tri, ci, di, pi));
}

bool is_in_circumcircle(const Point& v0, const Point& v1, const Point& v2, const Point& p) {
  using namespace predicates::adaptive;
  return incircle(&v0.x, &v1.x, &v2.x, &p.x) > 0.0;
}

void edge_swap(Triangulation& tri, uint32_t ti, uint32_t ti_op, const Edge& edge, uint32_t pi) {
  auto& t = tri.triangles[ti];
  auto& t_op = tri.triangles[ti_op];
  uint32_t ai = edge.ai;
  uint32_t bi = edge.bi;
  uint32_t ci = setdiff_edge(t_op, edge);
  t = require_ccw(tri, pi, bi, ci);
  t_op = require_ccw(tri, ai, pi, ci);
}

std::vector<uint32_t> divide_triangle(Triangulation& tri, const Point& point, uint32_t pi) {
  auto [ti0, ti1, neighbor_edge] = triangles_containing_point(tri, point);
  std::vector<uint32_t> ti_stack;
  uint32_t new_tris[4];
  uint32_t num_add;
  if (ti1 == invalid_index()) {
    add_point_in_triangle(tri, ti0, pi, new_tris);
    num_add = 3;
  } else {
    add_point_on_edge(tri, ti0, ti1, pi, neighbor_edge, new_tris);
    num_add = 4;
  }
  for (uint32_t i = 0; i < num_add; i++) {
    ti_stack.push_back(new_tris[i]);
  }
  return ti_stack;
}

std::tuple<uint32_t, uint32_t, uint32_t>
triangle_cutting_edge(const Triangulation& tri, const Edge& edge) {
  auto& ea = tri.points[edge.ai];
  auto& eb = tri.points[edge.bi];
  for (uint32_t ti = 0; ti < num_triangles(tri); ti++) {
    auto& t = tri.triangles[ti];
    int eai = find_point_index(t, edge.ai);
    if (eai == -1) {
      continue;
    }
    uint32_t ivu = t.i[cw(eai)];
    uint32_t ivl = t.i[ccw(eai)];
    auto& v0 = tri.points[ivu];
    auto& v1 = tri.points[ivl];
    int cw_side = hyperplane_side(ea, eb, v0);
    int ccw_side = hyperplane_side(ea, eb, v1);
    if (ccw_side < 0) {
      //  line a ----- b
      //          v1
      if (cw_side > 0) {
        //          v0
        //  line a ----- b
        //          v1
        return std::make_tuple(ti, ivu, ivl);
      }
      if (cw_side == 0) {
        //  b
        //  |
        //  v0 -- v1
        //cw(a)  ccw(a) < 0
        //  |  /
        //  | /
        //  |/
        //  a
        return std::make_tuple(invalid_index(), ivu, ivl);
      }
    }
  }
  assert(false);
  return std::make_tuple(invalid_index(), invalid_index(), invalid_index());
}

void triangulate_pseudo_polygon(Triangulation& tri, const uint32_t* pi_beg, const uint32_t* pi_end,
                                uint32_t ai, uint32_t bi) {
  assert(pi_end >= pi_beg && ai != bi);
  const auto pi_sz = uint32_t(pi_end - pi_beg);
  if (pi_sz == 0) {
    return;
  }
  uint32_t ci = pi_beg[0];
  uint32_t ci_ind = 0;
  for (uint32_t i = 1; i < pi_sz; i++) {
    auto& p = tri.points[pi_beg[i]];
    auto t = require_ccw(tri, ai, bi, ci);
    if (is_in_circumcircle(tri.points[t.i[0]], tri.points[t.i[1]], tri.points[t.i[2]], p)) {
      ci = pi_beg[i];
      ci_ind = i;
    }
  }
  const uint32_t* pe_beg = pi_beg;
  const uint32_t* pe_end = pi_beg + ci_ind;
  const uint32_t* pd_beg = pi_beg + ci_ind + 1;
  const uint32_t* pd_end = pi_end;
  triangulate_pseudo_polygon(tri, pe_beg, pe_end, ai, ci);
  triangulate_pseudo_polygon(tri, pd_beg, pd_end, ci, bi);
  add_triangle(tri, require_ccw(tri, ai, bi, ci));
}

void plus_edge_indices(Edge* e, uint32_t n) {
  e->ai += n;
  e->bi += n;
}

void sub_edge_indices(Edge* e, uint32_t n) {
  assert(e->ai >= n && e->bi >= n);
  e->ai -= n;
  e->bi -= n;
}

void remove_super_triangle_indices(std::vector<Edge>& es) {
  for (auto& e : es) {
    sub_edge_indices(&e, 3);
  }
}

void push_edge(std::vector<Edge>& edges, Edge edge) {
  edges.push_back(edge);
}

void add_edge(Triangulation& tri, Edge edge, std::vector<Edge>& edges) {
  assert(edge.ai < tri.points.size() && edge.bi < tri.points.size() && edge.ai != edge.bi);
  if (has_edge(tri, edge)) {
    push_edge(edges, edge);
    return;
  }

  std::vector<uint32_t> intersected_ti;
  std::vector<uint32_t> pu;
  std::vector<uint32_t> pl;
  {
    auto [ti0, piu0, pilu] = triangle_cutting_edge(tri, edge);
    if (ti0 == invalid_index()) {
      GROVE_LOG_WARNING_CAPTURE_META("Encountered edge.", "cdt");
      push_edge(edges, make_edge(edge.ai, piu0));
//      push_edge(edges, make_edge(piu0, pilu));
      add_edge(tri, make_edge(piu0, edge.bi), edges);
      return;
    }

    assert(ti0 != invalid_index());
    pu.push_back(piu0);
    pl.push_back(pilu);
    intersected_ti.push_back(ti0);
  }

  const auto& pa = tri.points[edge.ai];
  const auto& pb = tri.points[edge.bi];
  const uint32_t ai = edge.ai;
  const uint32_t bi = edge.bi;

  uint32_t target_bi = bi;
  uint32_t vi = ai;
  uint32_t ti = intersected_ti[0];
  uint32_t ivu = pu[0];
  uint32_t ivl = pl[0];
  while (true) {
    auto& t = tri.triangles[ti];
    if (has_point_index(t, target_bi)) {
      break;
    }

    auto [topi, shared_edge] = opposed_triangle(
      tri.triangles.data(), num_triangles(tri), ti, vi);
    assert(topi != invalid_index());
    intersected_ti.push_back(topi);

    uint32_t vopi = setdiff_edge(tri.triangles[topi], shared_edge);
    int vop_side = hyperplane_side(pa, pb, tri.points[vopi]);
    if (vop_side == 0) {
      assert(vopi == target_bi);  //  @TODO
      target_bi = vopi;
      break;
    } else if (vop_side > 0) {
      pu.push_back(vopi);
      vi = ivu;
      ivu = vopi;
    } else {
      pl.push_back(vopi);
      vi = ivl;
      ivl = vopi;
    }

    ti = topi;
  }

  std::sort(intersected_ti.begin(), intersected_ti.end(), std::greater<uint32_t>{});
  for (uint32_t rem_ti : intersected_ti) {
    tri.triangles.erase(tri.triangles.begin() + rem_ti);
  }

  triangulate_pseudo_polygon(tri, pu.data(), pu.data() + pu.size(), ai, target_bi);
  triangulate_pseudo_polygon(tri, pl.data(), pl.data() + pl.size(), ai, target_bi);

  if (target_bi != bi) {
    assert(false);  //  @TODO
    push_edge(edges, make_edge(ai, target_bi));
    add_edge(tri, make_edge(target_bi, bi), edges);
  } else {
    push_edge(edges, edge);
  }
}

} //  anon

TriPtLoc cdt::triangle_point_location(const Point& v0, const Point& v1, const Point& v2,
                                      const Point& a) {
  auto res = TriPtLoc::Inside;
  int e0_res = hyperplane_side(v0, v1, a);
  if (e0_res < 0) {
    return TriPtLoc::Outside;
  } else if (e0_res == 0) {
    res = TriPtLoc::Edge0;
  }
  int e1_res = hyperplane_side(v1, v2, a);
  if (e1_res < 0) {
    return TriPtLoc::Outside;
  } else if (e1_res == 0) {
    res = TriPtLoc::Edge1;
  }
  int e2_res = hyperplane_side(v2, v0, a);
  if (e2_res < 0) {
    return TriPtLoc::Outside;
  } else if (e2_res == 0) {
    res = TriPtLoc::Edge2;
  }
  return res;
}

int cdt::hyperplane_side(const Point& a, const Point& b, const Point& qp) {
  using namespace predicates::adaptive;
  double ori_res = orient2d(&a.x, &b.x, &qp.x);
  if (ori_res < 0.0) {
    return -1;
  } else if (ori_res == 0.0) {
    return 0;
  } else {
    return 1;
  }
}

void cdt::initialize_super_triangle(Triangulation& tri, const Point*, uint32_t) {
  //  @TODO
  assert(num_triangles(tri) == 0 && num_points(tri) == 0);
  const double scl = 2048.0;
  grove::add_point(tri, Point{-1.0, -1.0} * scl);
  grove::add_point(tri, Point{1.0, -1.0} * scl);
  grove::add_point(tri, Point{0.0, 1.0} * scl);
  grove::add_triangle(tri, make_triangle(0, 1, 2));
}

void cdt::remove_super_triangle(Triangulation& tri) {
  assert(num_points(tri) >= 3);
  tri.points.erase(tri.points.begin(), tri.points.begin() + 3);
  auto it = tri.triangles.begin();
  while (it != tri.triangles.end()) {
    auto& t = *it;
    if (has_point_index(t, 0) || has_point_index(t, 1) || has_point_index(t, 2)) {
      it = tri.triangles.erase(it);
    } else {
      for (auto& vi : t.i) {
        assert(vi >= 3);
        vi -= 3;
      }
      ++it;
    }
  }
}

void cdt::add_point(Triangulation& tri, const Point& point) {
  uint32_t pi = grove::add_point(tri, point);
  auto ti_stack = divide_triangle(tri, point, pi);
  while (!ti_stack.empty()) {
    uint32_t ti = ti_stack.back();
    ti_stack.pop_back();
    auto [ti_op, shared_edge] = opposed_triangle(
      tri.triangles.data(), num_triangles(tri), ti, pi);
    if (ti_op == invalid_index()) {
      continue;
    }
    if (!is_fixed_edge(tri, shared_edge)) {
      auto [p0, p1, p2] = read_vertices(tri.points.data(), tri.triangles[ti_op]);
      if (is_in_circumcircle(*p0, *p1, *p2, point)) {
        edge_swap(tri, ti, ti_op, shared_edge, pi);
        ti_stack.push_back(ti);
        ti_stack.push_back(ti_op);
      }
    }
  }
}

void cdt::add_points(Triangulation& tri, const Point* points, uint32_t num_points) {
  for (uint32_t i = 0; i < num_points; i++) {
    add_point(tri, points[i]);
  }
}

std::vector<Edge> cdt::add_edge(Triangulation& tri, Edge edge) {
  std::vector<Edge> res;
  plus_edge_indices(&edge, 3);  //  [0, 1, 2] for super tri
  grove::add_edge(tri, edge, res);
  remove_super_triangle_indices(res);
  return res;
}

std::vector<Edge> cdt::add_edges(Triangulation& tri, const Edge* edges, uint32_t num_edges) {
  std::vector<Edge> res;
  for (uint32_t i = 0; i < num_edges; i++) {
    auto edge = edges[i];
    plus_edge_indices(&edge, 3);  //  [0, 1, 2] for super tri
    grove::add_edge(tri, edge, res);
  }
  remove_super_triangle_indices(res);
  return res;
}

void cdt::validate(const Triangulation& tri) {
  for (auto& t : tri.triangles) {
    for (auto& vi : t.i) {
      assert(vi < tri.points.size());
      (void) vi;
    }
    assert(is_ccw(tri, t.i[0], t.i[1], t.i[2]));
  }
}

std::vector<uint32_t> cdt::find_excluding_hole(const Triangle* tris, uint32_t num_tris,
                                               const Edge* edges, uint32_t num_edges, uint32_t pvi) {
  std::vector<uint32_t> pend;
  std::unordered_set<uint32_t> visited;
  for (uint32_t ti = 0; ti < num_tris; ti++) {
    if (has_point_index(tris[ti], pvi)) {
      visited.insert(ti);
      pend.push_back(ti);
    }
  }
  assert(!pend.empty());
  std::vector<uint32_t> keep_ti;
  while (!pend.empty()) {
    auto ti = pend.back();
    pend.pop_back();
    keep_ti.push_back(ti);
    auto& t = tris[ti];
    for (uint32_t i = 0; i < 3; i++) {
      auto ei0 = i;
      auto ei1 = ccw(ei0);
      auto edge = make_edge(t.i[ei0], t.i[ei1]);
      if (!is_edge(edges, num_edges, edge)) {
        uint32_t adj_ti = adjacent_triangle(tris, num_tris, ti, edge);
        if (adj_ti != invalid_index() && visited.count(adj_ti) == 0) {
          visited.insert(adj_ti);
          pend.push_back(adj_ti);
        }
      }
    }
  }
#ifdef GROVE_DEBUG
  {
    std::unordered_set<uint32_t> kept;
    for (uint32_t ki : keep_ti) {
      assert(kept.count(ki) == 0);
      kept.insert(ki);
    }
  }
#endif
  return keep_ti;
}

std::vector<Triangle> cdt::remove_hole(const Triangle* tris, uint32_t num_tris,
                                       const Edge* edges, uint32_t num_edges, uint32_t num_points) {
  auto pvi = point_index_not_in_edges(edges, num_edges, num_points);
  assert(pvi != invalid_index());
  auto keep_at = find_excluding_hole(tris, num_tris, edges, num_edges, pvi);
  std::vector<Triangle> result(keep_at.size());
  keep_excluding_hole(tris, keep_at.data(), uint32_t(keep_at.size()), result.data());
  return result;
}

void cdt::keep_excluding_hole(const Triangle* tris, const uint32_t* keepi,
                              uint32_t num_keep, Triangle* out) {
  for (uint32_t i = 0; i < num_keep; i++) {
    out[i] = tris[keepi[i]];
  }
}

uint32_t cdt::point_index_not_in_edges(const Edge* edges, uint32_t num_edges, uint32_t num_points) {
  std::unordered_set<uint32_t> pi;
  for (uint32_t i = 0; i < num_edges; i++) {
    pi.insert(edges[i].ai);
    pi.insert(edges[i].bi);
  }
  for (uint32_t i = 0; i < num_points; i++) {
    if (pi.count(i) == 0) {
      return i;
    }
  }
  return invalid_index();
}

std::vector<Triangle> cdt::triangulate_simple(const Point* points, uint32_t num_points) {
  cdt::Triangulation tri;
  cdt::initialize_super_triangle(tri, points, num_points);
  cdt::add_points(tri, points, num_points);
  cdt::remove_super_triangle(tri);
  return tri.triangles;
}

std::vector<Triangle> cdt::triangulate_simple(const std::vector<Point>& points) {
  return triangulate_simple(points.data(), uint32_t(points.size()));
}

std::vector<Triangle> cdt::triangulate_remove_holes_simple(const Point* points, uint32_t num_points,
                                                           const Edge* edges, uint32_t num_edges) {
  cdt::Triangulation tri;
  cdt::initialize_super_triangle(tri, points, num_points);
  cdt::add_points(tri, points, num_points);
  auto es = cdt::add_edges(tri, edges, num_edges);
  cdt::remove_super_triangle(tri);
  return cdt::remove_hole(
    tri.triangles.data(),
    uint32_t(tri.triangles.size()),
    es.data(),
    uint32_t(es.size()),
    uint32_t(num_points));
}

std::vector<uint16_t> cdt::to_uint16_indices(const std::vector<Triangle>& tris) {
  std::vector<uint16_t> res(tris.size() * 3);
  uint32_t i{};
  for (auto& tri : tris) {
    for (auto& v : tri.i) {
      assert(v < (1u << 16u));
      res[i++] = uint16_t(v);
    }
  }
  return res;
}

GROVE_NAMESPACE_END
