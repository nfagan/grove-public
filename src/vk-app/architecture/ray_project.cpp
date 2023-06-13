#include "ray_project.hpp"
#include "ray_project_adjacency.hpp"
#include "grove/common/common.hpp"
#include "grove/math/constants.hpp"
#include "grove/math/triangle.hpp"
#include "grove/math/util.hpp"
#include <tuple>

GROVE_NAMESPACE_BEGIN

namespace {

using Vertex = Vec3<double>;
using ProjVertex = Vec2<double>;
using ProjVector = Vec2<double>;
using NonAdjacentConnections = ray_project::NonAdjacentConnections;

struct ProjTriangle {
  uint32_t i0;
  uint32_t i1;
  uint32_t i2;
};

struct Edge {
  uint32_t i0;
  uint32_t i1;
};

struct Frame {
  Vertex x;
  Vertex y;
  Vertex z;
};

struct TriangleContext {
  Vertex p0;
  Vertex p1;
  Vertex p2;
  ProjVertex fp0;
  ProjVertex fp1;
  ProjVertex fp2;
  Frame f;
  Frame fi;
};

template <typename T>
bool line_line_2d_intersect(const Vec2<T>& p0, const Vec2<T>& p1,
                            const Vec2<T>& p2, const Vec2<T>& p3,
                            T* t, T* u) {
  //  https://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect
  *t = 0.0;
  *u = 0.0;

  auto x2d = [](const Vec2<T>& v, const Vec2<T>& w) {
    return v.x * w.y - v.y * w.x;
  };

  auto p = p0;
  auto r = p1 - p0;
  auto q = p2;
  auto s = p3 - p2;
  auto rxs = x2d(r, s);
  auto qp = q - p;
  auto qpxs = x2d(qp, s);
  auto qpxr = x2d(qp, r);

  bool hit = false;
  if (rxs == T(0) && qpxr == T(0)) {
    auto rr = dot(r, r);
    auto t0 = dot(qp, r) / rr;
    auto t1 = t0 + dot(s, r) / rr;
    if (t0 > t1) {
      assert(dot(s, r) < 0);
      std::swap(t0, t1);
    }
    if (t0 < T(0)) {
      hit = t1 >= T(0);
    } else {
      hit = t0 <= T(1);
    }
  } else if (rxs == T(0) && qpxr != T(0)) {
    //
  } else if (rxs != T(0)) {
    *t = qpxs / rxs;
    *u = qpxr / rxs;
    hit = *t >= T(0) && *t <= T(1) && *u >= T(0) && *u <= T(1);
  }

  return hit;
}

Vertex to_vertex(const Vec3f& v) {
  return Vertex{v.x, v.y, v.z};
}

ProjVector theta_to_ray_direction(double theta) {
  return ProjVertex{std::cos(theta), std::sin(theta)};
}

void extract_vertices(const ProjTriangle& tri, const Vec3f* ps,
                      Vertex* p0, Vertex* p1, Vertex* p2) {
  *p0 = to_vertex(ps[tri.i0]);
  *p1 = to_vertex(ps[tri.i1]);
  *p2 = to_vertex(ps[tri.i2]);
}

ProjVertex apply_inverse_frame(const Frame& f, const Vertex& p0, const Vertex& p) {
  auto eval_p = p - p0;
  auto proj_p = f.x * eval_p.x + f.y * eval_p.y + f.z * eval_p.z;
  return ProjVertex{proj_p.x, proj_p.y};
}

//  Transposed frame.
Frame invert_frame(const Frame& f) {
  Frame result;
  result.x = Vertex{f.x.x, f.y.x, f.z.x};
  result.y = Vertex{f.x.y, f.y.y, f.z.y};
  result.z = Vertex{f.x.z, f.y.z, f.z.z};
  return result;
}

//  Frame taking projected space vertices to world space vertices.
Frame compute_frame(const Vertex& p0, const Vertex& p1, const Vertex& p2) {
  auto x = normalize(p1 - p0);
  auto v = normalize(p2 - p0);
  auto n = cross(v, x);
  auto y = cross(x, n);
  y = normalize(y);
  n = normalize(n);
  return {x, y, n};
}

ProjTriangle shift_ccw(const ProjTriangle& t) {
  return {t.i2, t.i0, t.i1};
}

ProjTriangle shift_cw(const ProjTriangle& t) {
  return {t.i1, t.i2, t.i0};
}

double maximum_edge_length(const ProjVertex& p0, const ProjVertex& p1, const ProjVertex& p2) {
  auto l0 = (p1 - p0).length();
  auto l1 = (p2 - p1).length();
  auto l2 = (p2 - p0).length();
  return std::max(l2, std::max(l0, l1));
}

TriangleContext make_triangle_context(const ProjTriangle& tri, const Vec3f* ps) {
  TriangleContext result;
  extract_vertices(tri, ps, &result.p0, &result.p1, &result.p2);
  result.f = compute_frame(result.p0, result.p1, result.p2);
  result.fi = invert_frame(result.f);
  result.fp0 = {};  //  always (0, 0)
  result.fp1 = apply_inverse_frame(result.fi, result.p0, result.p1);
  result.fp2 = apply_inverse_frame(result.fi, result.p0, result.p2);
  return result;
}

auto find_next_intersecting_edge(const TriangleContext& ctx, const ProjVertex& rp,
                                 const ProjVertex& query_p, const ProjTriangle& tri) {
  struct Result {
    ProjVector intersect_v;
    Edge adj_edge;
    double exit_t;
    Vertex exit_p;
  };

  double t_left;
  double u_left;
  double t_right;
  double u_right;
  bool hit_left = line_line_2d_intersect(ctx.fp2, ctx.fp0, rp, query_p, &t_left, &u_left);
  bool hit_right = line_line_2d_intersect(ctx.fp1, ctx.fp2, rp, query_p, &t_right, &u_right);
  Result result;
  if (hit_left) {
    result.intersect_v = ctx.fp0 - ctx.fp2;
    result.adj_edge = Edge{tri.i2, tri.i0};
    result.exit_t = t_left;
    result.exit_p = (ctx.p0 - ctx.p2) * t_left + ctx.p2;
  } else {
    //  @TODO: Edge-on intersections are not handled properly currently. We should probably at
    //  least add `success` flag or something so that the whole routine can abort in that case.
    assert(hit_right);
    (void) hit_right;
    result.intersect_v = ctx.fp2 - ctx.fp1;
    result.adj_edge = Edge{tri.i1, tri.i2};
    result.exit_t = t_right;
    result.exit_p = (ctx.p2 - ctx.p1) * t_right + ctx.p1;
  }
  return result;
}

auto maybe_traverse_to_non_adjacent(const NonAdjacentConnections* non_adjacent,
                                    uint32_t ti, const Edge& adj_edge, const Vec3f* ps,
                                    double rt) {
  struct Result {
    bool success;
    uint32_t adj_ti;
    Edge adj_edge;
    double rt;
  };
  Result result{};

  auto non_adjacent_key = ray_project::make_non_adjacent_connection_key(
    ti, adj_edge.i0, adj_edge.i1);
  auto it = ray_project::find_non_adjacent_connections(
    non_adjacent, non_adjacent_key);

  auto* candidate_entry = it.begin;
  double eval_rt = rt;
  for (; candidate_entry != it.end; ++candidate_entry) {
    auto& src_edge = candidate_entry->src.edge;
    if (src_edge.i0 != adj_edge.i0) {
      //  `rt` is inverted because the adjacent edge is flipped with respect to its
      //  encoding in `src_edge`
      assert(src_edge.i0 == adj_edge.i1 && src_edge.i1 == adj_edge.i0);
      eval_rt = 1.0 - rt;
    }
    const Vec2f& targ_coords = candidate_entry->target_edge_fractional_coordinates;
    if (eval_rt >= targ_coords.x && eval_rt < targ_coords.y) {
      //  candidate within target range.
      break;
    }
  }

  if (candidate_entry == it.end) {
    //  failed to find candidate
    return result;
  }

  const Vec3f& src_p0 = ps[candidate_entry->src.edge.i0];
  const Vec3f& src_p1 = ps[candidate_entry->src.edge.i1];
  const Vec3f& targ_p0 = ps[candidate_entry->target.edge.i0];
  const Vec3f& targ_p1 = ps[candidate_entry->target.edge.i1];

  auto src_v = src_p1 - src_p0;
  auto src_len = src_v.length();
  auto targ_len = (targ_p1 - targ_p0).length();

  const auto min_frac_coord = double(candidate_entry->target_edge_fractional_coordinates.x);
  const double old_p = (eval_rt - min_frac_coord) * double(src_len);
  double new_rt = old_p / double(targ_len);
  assert(new_rt >= 0.0 && new_rt <= 1.0);
  new_rt = clamp(new_rt, 0.0, 1.0);

  const auto& targ_edge = candidate_entry->target.edge;
  result.success = true;
  result.adj_ti = candidate_entry->target.ti;
  result.adj_edge = Edge{targ_edge.i0, targ_edge.i1};
  result.rt = new_rt;
  return result;
}

uint32_t adjacent_triangle(const uint32_t* tris, uint32_t num_tris, uint32_t ti, const Edge& edge) {
  assert(edge.i0 != edge.i1);
  for (uint32_t i = 0; i < num_tris; i++) {
    if (i != ti) {
      uint32_t match_count{};
      for (uint32_t j = 0; j < 3; j++) {
        uint32_t ind = tris[i * 3 + j];
        if (edge.i0 == ind) {
          match_count++;
        }
        if (edge.i1 == ind) {
          match_count++;
        }
      }
      if (match_count == 2) {
        return i;
      }
    }
  }
  return tri::no_adjacent_triangle();
}

auto find_next_triangle(uint32_t ti, Edge adj_edge, double rt,
                        const uint32_t* tris, uint32_t num_tris, const Vec3f* ps,
                        const ProjectRayEdgeIndices* edge_indices,
                        const NonAdjacentConnections* non_adjacent) {
  struct Result {
    double rt;
    Edge adj_edge;
    uint32_t adj_ti;
  };

  Result result{};

  uint32_t adj_ti;
  if (edge_indices) {
    adj_ti = tri::find_adjacent(*edge_indices, ti, adj_edge.i0, adj_edge.i1);
#ifdef GROVE_DEBUG
    assert(adj_ti == adjacent_triangle(tris, num_tris, ti, adj_edge));
#endif
  } else {
    adj_ti = adjacent_triangle(tris, num_tris, ti, adj_edge);
  }

  if (non_adjacent && adj_ti == tri::no_adjacent_triangle()) {
    auto adj_res = maybe_traverse_to_non_adjacent(non_adjacent, ti, adj_edge, ps, rt);
    if (adj_res.success) {
      adj_ti = adj_res.adj_ti;
      adj_edge = adj_res.adj_edge;
      rt = adj_res.rt;
    }
  }

  result.adj_ti = adj_ti;
  result.adj_edge = adj_edge;
  result.rt = rt;
  return result;
}

int edge_index(const ProjTriangle& t, uint32_t i) {
  int ei{};
  for (uint32_t ind : {t.i0, t.i1, t.i2}) {
    if (ind == i) {
      return ei;
    }
    ei++;
  }
  return -1;
}

bool shift_triangle_by_edge(ProjTriangle* tri, const Edge& edge) {
  bool need_invert = false;
  while (true) {
    auto i0 = edge_index(*tri, edge.i0);
    auto i1 = edge_index(*tri, edge.i1);
    if ((i0 == 0 && i1 == 1) || (i0 == 1 && i1 == 0)) {
      need_invert = i0 == 1 && i1 == 0;
      break;
    }
    *tri = shift_cw(*tri);
  }
  return need_invert;
}

ProjTriangle to_proj_triangle(const uint32_t* tris, uint32_t ti) {
  return ProjTriangle{tris[ti * 3], tris[ti * 3 + 1], tris[ti * 3 + 2]};
}

bool require_ray_direction_flip(const Vec3f* ps,
                                const ProjTriangle& prev_tri,
                                const ProjTriangle& next_tri) {
  Vec3f curr_n;
  tri::compute_normals_per_triangle(&prev_tri.i0, 1, ps, &curr_n);
  Vec3f next_n;
  tri::compute_normals_per_triangle(&next_tri.i0, 1, ps, &next_n);
  return dot(curr_n, next_n) <= 0.0f;
}

ProjectRayResultEntry make_result_entry(const Vertex& entry_p,
                                        const Vertex& exit_p,
                                        uint32_t ti,
                                        const ProjTriangle& tri,
                                        double theta,
                                        bool required_flip) {
  ProjectRayResultEntry result;
  result.entry_p = entry_p;
  result.exit_p = exit_p;
  result.ti = ti;
  result.tri[0] = tri.i0;
  result.tri[1] = tri.i1;
  result.tri[2] = tri.i2;
  result.theta = theta;
  result.required_flip = required_flip;
  return result;
}

} //  anon

ProjectRayResult project_ray_onto_mesh(const uint32_t* tris, uint32_t num_tris,
                                       const Vec3f* ps, const uint32_t* src_tri,
                                       uint32_t src_ti, const Vec3<double>& src_p,
                                       double ray_theta, double ray_len,
                                       const ProjectRayEdgeIndices* edge_indices,
                                       const NonAdjacentConnections* non_adjacent_connections) {
  ProjTriangle tri{src_tri[0], src_tri[1], src_tri[2]};
  uint32_t ti = src_ti;
  double remaining_len = ray_len;

  ProjectRayResult result{};
  bool required_flip = false;
  uint32_t iter{};
  uint32_t max_iter{~0u}; //  @TODO: Expose as parameter.

  double rt{};
  while (remaining_len > 0.0 && iter < max_iter) {
    auto ctx = make_triangle_context(tri, ps);
    ProjVertex rp;
    if (iter == 0) {
      rp = apply_inverse_frame(ctx.fi, ctx.p0, src_p);
      while (ray_theta < 0.0 || ray_theta > pi()) {
        if (ray_theta < 0.0) {
          //  left edge vector would be fp2 - fp0, but fp0 is [0, 0]
          auto new_theta = std::atan2(ctx.fp2.y, ctx.fp2.x);
          ray_theta = (pi() - std::abs(ray_theta)) - new_theta;
          tri = shift_ccw(tri);
        } else {
          //  right edge
          auto fv = ctx.fp2 - ctx.fp1;
          auto new_theta = std::atan2(fv.y, fv.x);
          ray_theta = ray_theta - new_theta;
          tri = shift_cw(tri);
        }
        ctx = make_triangle_context(tri, ps);
        rp = apply_inverse_frame(ctx.fi, ctx.p0, src_p);
      }
    } else {
      rp = (ctx.fp1 - ctx.fp0) * rt + ctx.fp0;
    }

    assert(ray_theta >= 0.0 && ray_theta <= pi());
    ProjVector rd = theta_to_ray_direction(ray_theta);
    double query_length = maximum_edge_length(ctx.fp0, ctx.fp1, ctx.fp2) * 4.0;
    ProjVertex query_p = rp + rd * query_length;

    auto edge_isect = find_next_intersecting_edge(ctx, rp, query_p, tri);
    Vertex entry_p;
    if (iter == 0) {
      entry_p = src_p;
    } else {
      entry_p = (ctx.p1 - ctx.p0) * rt + ctx.p0;
    }

    auto traversed_len = (edge_isect.exit_p - entry_p).length();
    if (remaining_len <= traversed_len) {
      auto exit_ray = edge_isect.exit_p - entry_p;
      auto exit_rt = remaining_len / traversed_len;
      edge_isect.exit_p = exit_ray * exit_rt + entry_p;
      remaining_len = 0.0;
    } else {
      remaining_len -= traversed_len;
    }

    result.entries.push_back(make_result_entry(
      entry_p, edge_isect.exit_p, ti, tri, ray_theta, required_flip));

    const auto next_tri = find_next_triangle(
      ti, edge_isect.adj_edge, edge_isect.exit_t,
      tris, num_tris, ps, edge_indices, non_adjacent_connections);

    const uint32_t adj_ti = next_tri.adj_ti;
    const Edge adj_edge = next_tri.adj_edge;
    rt = next_tri.rt;

    if (adj_ti == tri::no_adjacent_triangle()) {
      break;
    }

    tri = to_proj_triangle(tris, adj_ti);
    const uint32_t prev_ti = ti;
    ti = adj_ti;

    if (shift_triangle_by_edge(&tri, adj_edge)) {
      rt = 1.0 - rt;
    }

    auto neg_norm_int_v = -edge_isect.intersect_v;
    auto next_theta = std::atan2(neg_norm_int_v.y, neg_norm_int_v.x);
    ray_theta -= next_theta;

    required_flip = require_ray_direction_flip(
      ps, to_proj_triangle(tris, prev_ti), to_proj_triangle(tris, ti));
    if (required_flip) {
      ray_theta = pi() - ray_theta;
    }

    iter++;
  }

  const bool completed = remaining_len == 0.0;
  result.traversed_length = ray_len - remaining_len;
  result.completed = completed;
  return result;
}

ProjectRayResult project_ray_onto_mesh(const uint32_t* tris, uint32_t num_tris, const Vec3f* ps,
                                       const ProjectRayNextIteration& next, double ray_len,
                                       const ProjectRayEdgeIndices* edge_indices,
                                       const NonAdjacentConnections* non_adjacent_connections) {
  return project_ray_onto_mesh(
    tris, num_tris, ps, next.tri, next.ti, next.p, next.ray_theta, ray_len,
    edge_indices, non_adjacent_connections);
}

Vec3<double> edge_uv_to_world_point(const Vec3f& p0, const Vec3f& p1, const Vec3f& p2,
                                    const Vec2f& uv) {
  auto p0d = to_vertex(p0);
  auto p1d = to_vertex(p1);
  auto p2d = to_vertex(p2);
  auto f = compute_frame(p0d, p1d, p2d);
  auto fi = invert_frame(f);
  auto fp1 = apply_inverse_frame(fi, p0d, p1d);
  auto fp2 = apply_inverse_frame(fi, p0d, p2d);
  auto fx0 = fp1 * double(uv.x);
  auto fx1 = fp2 * double(uv.x);
  auto p = lerp(double(uv.y), fx0, fx1);
  return (f.x * p.x + f.y * p.y) + p0d;
}

Vec3f transform_vector_to_projected_triangle_space(const Vec3f& p0, const Vec3f& p1,
                                                   const Vec3f& p2, const Vec3f& v) {
  auto p0d = to_vertex(p0);
  auto p1d = to_vertex(p1);
  auto p2d = to_vertex(p2);
  auto f = compute_frame(p0d, p1d, p2d);
  auto fi = invert_frame(f);
  auto proj_v = apply_inverse_frame(fi, Vertex{}, to_vertex(v));
  return Vec3f{float(proj_v.x), float(proj_v.y), 0.0f};
}

ProjectRayNextIteration prepare_next_iteration(const ProjectRayResult& res, double theta_offset) {
  assert(!res.entries.empty());
  auto& entry = res.entries.back();
  ProjectRayNextIteration result;
  result.tri[0] = entry.tri[0];
  result.tri[1] = entry.tri[1];
  result.tri[2] = entry.tri[2];
  result.ti = entry.ti;
  result.p = entry.exit_p;
  result.ray_theta = entry.theta + theta_offset;
  return result;
}

GROVE_NAMESPACE_END
