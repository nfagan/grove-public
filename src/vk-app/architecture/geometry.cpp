#include "geometry.hpp"
#include "grove/math/util.hpp"
#include "grove/math/matrix_transform.hpp"
#include "grove/math/triangle.hpp"
#include "grove/common/common.hpp"
#include "grove/common/memory.hpp"
#include <unordered_map>
#include <array>

GROVE_NAMESPACE_BEGIN

namespace {

using namespace arch;

struct PendingConstraint {
  float x;
  float segment;
  uint32_t ti;
};

struct PendingConstraintLess {
  bool operator()(const PendingConstraint& a, const PendingConstraint& b) const noexcept {
    return std::tie(a.segment, a.x) < std::tie(b.segment, b.x);
  }
};

PendingConstraint make_pending_constraint(float x, float segment, uint32_t ti) {
  return {x, segment, ti};
}

cdt::Triangle remap_triangle(const cdt::Triangle& t,
                             const std::unordered_map<uint32_t, uint32_t>& map) {
  return cdt::Triangle{{
    map.at(t.i[0]),
    map.at(t.i[1]),
    map.at(t.i[2]),
  }};
}

template <typename T, typename Hash>
struct PointStore {
  std::unordered_map<T, uint32_t, Hash> mapped;
  std::vector<T> points;
};

struct Context {
  PointStore<cdt::Point, cdt::HashPoint<double>> pending_tri;
  std::unordered_map<uint32_t, uint32_t> tri_to_tot;
  std::vector<Vec3f> tot_ps;
  std::vector<Vec3f> tot_ns;
  std::vector<cdt::Triangle> tot_ts;
  std::vector<cdt::Edge> tot_cs;
  std::vector<Vec3f> scratch_ps;
  std::vector<Vec3f> scratch_ns;
  std::vector<uint32_t> scratch_cts;
  std::unordered_set<uint32_t> interior_edge_points;
  float aspect_ratio{1.0f};
};

constexpr float p3_eps() {
  return 1e-7f;
}

constexpr Vec3f wall_face_normal() {
  return Vec3f{0.0f, -1.0f, 0.0f};
}

template <typename T, typename Hash>
std::tuple<bool, uint32_t> require_point(PointStore<T, Hash>& pend, const T& pt) {
  if (auto it = pend.mapped.find(pt); it != pend.mapped.end()) {
    return std::make_tuple(false, it->second);
  } else {
    auto ind = uint32_t(pend.points.size());
    pend.mapped[pt] = ind;
    pend.points.push_back(pt);
    return std::make_tuple(true, ind);
  }
}

template <typename T, typename U>
std::tuple<bool, uint32_t> require_point(std::vector<T>& pend, T* pt, U eps, int sz) {
  uint32_t pi{};
  for (auto& p : pend) {
    auto delta = abs(p - *pt);
    bool within_thresh = true;
    for (int i = 0; i < sz; i++) {
      if (delta[i] >= eps) {
        within_thresh = false;
        break;
      }
    }
    if (within_thresh) {
      *pt = p;
      return std::make_tuple(false, pi);
    }
    pi++;
  }
  auto ind = uint32_t(pend.size());
  pend.push_back(*pt);
  return std::make_tuple(true, ind);
}

std::vector<cdt::Triangle> triangulate(const std::vector<cdt::Point>& p) {
  return cdt::triangulate_simple(p.data(), uint32_t(p.size()));
}

std::vector<cdt::Triangle> triangulate_remove_holes(const std::vector<cdt::Point>& p,
                                                    const std::vector<cdt::Edge>& e) {
  return cdt::triangulate_remove_holes_simple(
    p.data(), uint32_t(p.size()), e.data(), uint32_t(e.size()));
}

std::vector<cdt::Point> to_points(const std::vector<Vec2f>& ps) {
  std::vector<cdt::Point> res(ps.size());
  for (uint32_t i = 0; i < uint32_t(ps.size()); i++) {
    res[i] = Vec2<double>{ps[i].x, ps[i].y};
  }
  return res;
}

cdt::Point to_point_xz(const Vec3f& p) {
  return cdt::Point{p.x, p.z};
}

float z_curl(float fy, float fcurl) {
  return std::pow(fy, 0.25f) * fcurl * 0.5f;
}

float top_offset(float) {
  return 2.0f;
}

Vec2f frac_radial_point(float f, float radius) {
  auto theta = f * pif();
  return Vec2f{-std::sin(theta), std::cos(theta)} * radius;
}

Vec2f curved_vertical_connection(const Vec2<double>& p2,
                                 const CurvedVerticalConnectionParams& params) {
  auto x = float(p2.x);
  float ex = params.target_lower ? (1.0f - x) : x;
  auto y = float(p2.y);
  if (y >= params.min_y) {
    float yt = y - params.min_y;
    y = params.min_y + yt * std::pow(ex, params.power);
  }
  return Vec2f{x, y};
}

Vec3f to_wall_space(Vec3f p3, const WallHole& hole) {
  p3.z -= 1.0f;
  p3.x *= hole.scale.x * 0.5f;
  p3.z *= hole.scale.y * 0.5f;
  auto rot = make_rotation(hole.rot);
  Vec2f rot_p{p3.x, p3.z};
  rot_p = rot * rot_p;
  rot_p += hole.off + Vec2f{0.5f};
  p3.x = rot_p.x;
  p3.z = rot_p.y;
  return p3;
}

Vec3f curved_segment(int si, const Vec2f& p01, const WallHole& hole) {
  float fx = p01.x;
  float fy = p01.y;

  float nc_scale = 1.0f - hole.curl;
  auto zc = z_curl(fy, hole.curl);
  Vec3f base_p{-nc_scale, fy, zc};
  Vec3f top_p{-1.0f + zc, fy, hole.curl};
  auto p3 = lerp(fx, base_p, top_p);
  auto top_off = top_offset(hole.curl);

  if (fx != 0.0f && fx != 1.0f) {
    auto scl = std::sqrt(2.0f) / 6.0f;
    auto dn = normalize(Vec2f{-1.0f, -1.0f});
    auto mag = std::sin(fx * pif());
    p3.x += mag * dn.x * scl * hole.curl;
    p3.z += mag * dn.y * scl * hole.curl;
  }

  if (si == 1) {
    p3.x = -p3.x;
  } else if (si == 2) {
    p3.x = -p3.x;
    p3.z = -p3.z + top_off;
  } else if (si == 3) {
    p3.z = -p3.z + top_off;
  } else {
    assert(si == 0);
  }

  return to_wall_space(p3, hole);
}

Vec3f straight_segment(int si, const Vec2f& p01, const WallHole& hole) {
  float fx11 = p01.x * 2.0f - 1.0f;
  float fy = p01.y;
  float nc_scale = 1.0f - hole.curl;
  Vec3f p3{fx11 * nc_scale, fy, z_curl(fy, hole.curl)};
  auto top_off = top_offset(hole.curl);
  if (si == 1) {
    p3.z = -p3.z;
    p3.x += 1.0f;
    p3.z += 1.0f;
    std::swap(p3.x, p3.z);
  } else if (si == 2) {
    p3.z = -p3.z + top_off;
  } else if (si == 3) {
    p3.x += 1.0f;
    p3.z -= 1.0f;
    std::swap(p3.x, p3.z);
  } else {
    assert(si == 0);
  }
  return to_wall_space(p3, hole);
}

std::tuple<Vec2f, Vec2f> adjoining_curved_segment(const Vec2f& p0, const Vec2f& p1,
                                                  const Vec2f& v0, const Vec2f& v1,
                                                  const Vec2f& n0, const Vec2f& n1, float f) {
  if (f == 0.0f) {
    return {p0, n0};
  } else if (f == 1.0f) {
    return {p1, n1};
  }
  float l_v0 = v0.length();
  float l_v1 = v1.length();
  Vec2f n_v0 = v0 / l_v0;
  Vec2f n_v1 = v1 / l_v1;
  float th = std::acos(dot(n_v1, n_v0));
  float rot = th * f;
  Vec2f n = make_rotation(-rot) * n_v0;
  float len = lerp(f, l_v0, l_v1);
  return {p0 - v0 + n * len, n};
}

void scratch_compute_normals(Context& ctx, const std::vector<cdt::Triangle>& ts,
                             uint32_t num_points, float sign) {
  std::fill(ctx.scratch_cts.begin(), ctx.scratch_cts.begin() + num_points, 0u);
  tri::compute_normals(
    reinterpret_cast<const uint32_t*>(ts.data()),
    uint32_t(ts.size()),
    ctx.scratch_ps.data(),
    ctx.scratch_ns.data(),
    ctx.scratch_cts.data());
  if (sign != 1.0f) {
    for (uint32_t i = 0; i < num_points; i++) {
      ctx.scratch_ns[i] *= sign;
    }
  }
}

void require_normal(Context& ctx, uint32_t pi, bool is_new) {
  if (is_new) {
    ctx.tot_ns.push_back(ctx.scratch_ns[pi]);
  }
}

void add_hole(const std::vector<Vec2f>& sseg_ps,
              const std::vector<cdt::Triangle>& sseg_ts,
              const std::vector<Vec2f>& cseg_ps,
              const std::vector<cdt::Triangle>& cseg_ts,
              const WallHole& hole,
              Context& ctx) {
  std::vector<PendingConstraint> pending_constraints;
  std::unordered_map<uint32_t, uint32_t> tmp_map;
  for (int si = 0; si < 4; si++) {
    //  straight
    {
      uint32_t pi{};
      for (auto& p : sseg_ps) {
        ctx.scratch_ps[pi++] = straight_segment(si, p, hole);
      }
      const float sign = (si == 2 || si == 3) ? -1.0f : 1.0f;
      scratch_compute_normals(ctx, sseg_ts, pi, sign);
    }
    {
      uint32_t pi{};
      for (auto& p : sseg_ps) {
        auto p3 = ctx.scratch_ps[pi];
        auto [is_new_tot_p, ind] = require_point(ctx.tot_ps, &p3, p3_eps(), 3);
        assert(tmp_map.count(pi) == 0);
        tmp_map[pi] = ind;
        //  normal
        require_normal(ctx, pi, is_new_tot_p);
        if (p.y == 0.0f) {
          //  perimeter
          ctx.tot_ns[ind] = wall_face_normal(); //  use wall normal.
          auto [is_new_tri_p, tri_i] = require_point(ctx.pending_tri, to_point_xz(p3));
          if (is_new_tri_p) {
            float x = p.x;
            if (si == 2 || si == 3) {
              x = 1.0f - x;
            }
            ctx.tri_to_tot[tri_i] = ind;
            pending_constraints.push_back(
              make_pending_constraint(x, float(si) + 0.5f, tri_i));
          }
        } else if (p.y == 1.0f) {
          ctx.interior_edge_points.insert(ind);
        }
        pi++;
      }
      for (auto& t : sseg_ts) {
        ctx.tot_ts.push_back(remap_triangle(t, tmp_map));
      }
      tmp_map.clear();
    }
    //  curved
    {
      uint32_t pi{};
      for (auto& p : cseg_ps) {
        ctx.scratch_ps[pi++] = curved_segment(si, p, hole);
      }
      const float sign = (si == 0 || si == 2) ? -1.0f : 1.0f;
      scratch_compute_normals(ctx, cseg_ts, pi, sign);
    }
    {
      uint32_t pi{};
      for (auto& p : cseg_ps) {
        auto p3 = ctx.scratch_ps[pi];
        auto [is_new_tot_p, ind] = require_point(ctx.tot_ps, &p3, p3_eps(), 3);
        assert(tmp_map.count(pi) == 0);
        tmp_map[pi] = ind;
        require_normal(ctx, pi, is_new_tot_p);
        if (p.y == 0.0f) {
          //  perimeter
          ctx.tot_ns[ind] = wall_face_normal(); //  use wall normal.
          auto [is_new_tri_p, tri_i] = require_point(ctx.pending_tri, to_point_xz(p3));
          if (is_new_tri_p) {
            float x = p.x;
            if (si == 0 || si == 2) {
              x = 1.0f - x;
            }
            ctx.tri_to_tot[tri_i] = ind;
            pending_constraints.push_back(
              make_pending_constraint(x, float(si), tri_i));
          }
        } else if (p.y == 1.0f) {
          ctx.interior_edge_points.insert(ind);
        }
        pi++;
      }
      for (auto& t : cseg_ts) {
        ctx.tot_ts.push_back(remap_triangle(t, tmp_map));
      }
      tmp_map.clear();
    }
  }

  std::sort(pending_constraints.begin(), pending_constraints.end(), PendingConstraintLess{});
  for (uint32_t i = 0; i < uint32_t(pending_constraints.size()); i++) {
    auto i1 = i == (pending_constraints.size() - 1) ? 0 : i + 1;
    cdt::Edge edge{pending_constraints[i].ti, pending_constraints[i1].ti};
    ctx.tot_cs.push_back(edge);
  }
}

void add_background_grid(const std::vector<Vec2f>& grid_ps, Context& ctx, WallHoleResult& res) {
  for (auto& p : grid_ps) {
    Vec3f p3{p.x * ctx.aspect_ratio, 0.0f, p.y};
    auto [is_new_tot_p, ind] = require_point(ctx.tot_ps, &p3, p3_eps(), 3);
    auto [is_new_tri_p, tri_i] = require_point(ctx.pending_tri, to_point_xz(p3));
    if (is_new_tot_p) {
      ctx.tot_ns.push_back(wall_face_normal());
    }
    if (is_new_tri_p) {
      ctx.tri_to_tot[tri_i] = ind;
    }
    if (p.x == 0.0f && p.y == 0.0f) {
      res.bot_l_ind = ind;
    } else if (p.x == 1.0f && p.y == 0.0f) {
      res.bot_r_ind = ind;
    } else if (p.x == 1.0f && p.y == 1.0f) {
      res.top_r_ind = ind;
    } else if (p.x == 0.0f && p.y == 1.0f) {
      res.top_l_ind = ind;
    }
  }
}

void init_context(Context* ctx, uint32_t max_num_ps, float ar) {
  ctx->scratch_ps.resize(max_num_ps);
  ctx->scratch_ns.resize(max_num_ps);
  ctx->scratch_cts.resize(max_num_ps);
  ctx->aspect_ratio = ar;
}

Vec3f transform_to_obb(Vec3f p, const OBB3f& obb, const Vec3f& size_scl, const Vec3f& offset_scl) {
  p *= obb.half_size * size_scl;
  p = orient(obb, p);
  p += obb.position;
  p -= orient(obb, obb.half_size * offset_scl);
  return p;
}

void transform_normals_to_obb(void* normals, size_t num_verts, const OBB3f& obb) {
  auto* ns = static_cast<unsigned char*>(normals);
  for (size_t i = 0; i < num_verts; i++) {
    Vec3f n;
    memcpy(&n, ns + i * sizeof(Vec3f), sizeof(Vec3f));
#if 0
    n = normalize(n / obb.half_size);
#endif
    n = orient(obb, n);
    memcpy(ns + i * sizeof(Vec3f), &n, sizeof(Vec3f));
  }
}

void transform_positions_to_obb(void* positions, size_t num_verts,
                                const OBB3f& obb, const Vec3f& size_scl, const Vec3f& offset_scl) {
  auto* ps = static_cast<unsigned char*>(positions);
  for (size_t i = 0; i < num_verts; i++) {
    Vec3f v;
    memcpy(&v, ps + i * sizeof(Vec3f), sizeof(Vec3f));
    v = transform_to_obb(v, obb, size_scl, offset_scl);
    memcpy(ps + i * sizeof(Vec3f), &v, sizeof(Vec3f));
  }
}

OBB3f negate_k(const OBB3f& src) {
  auto dst = src;
  dst.k = -dst.k;
  return dst;
}

void normalize_vec3_to_01(unsigned char* p, size_t num_points,
                          const bool invert[3], const int perm[3]) {
  Vec3f mins{infinityf()};
  Vec3f maxs{-infinityf()};
  for (size_t i = 0; i < num_points; i++) {
    Vec3f v;
    memcpy(&v, p + i * sizeof(Vec3f), sizeof(Vec3f));
    for (int j = 0; j < 3; j++) {
      if (v[j] < mins[j]) {
        mins[j] = v[j];
      }
      if (v[j] > maxs[j]) {
        maxs[j] = v[j];
      }
    }
  }
  for (size_t i = 0; i < num_points; i++) {
    Vec3f v;
    auto dst_ind = i * sizeof(Vec3f);
    memcpy(&v, p + dst_ind, sizeof(Vec3f));
    for (int j = 0; j < 3; j++) {
      float span = maxs[j] - mins[j];
      v[j] = v[j] == mins[j] ? 0.0f : v[j] == maxs[j] ? 1.0f : (v[j] - mins[j]) / span;
      if (invert[j]) {
        v[j] = 1.0f - v[j];
      }
    }
    v = permute(v, perm[0], perm[1], perm[2]);
    memcpy(p + dst_ind, &v, sizeof(Vec3f));
  }
}

void push_indices(LinearAllocator* alloc, const TriangulatedGrid& grid,
                  uint32_t* offset, uint32_t* num_inds) {
  for (uint32_t i = 0; i < grid.num_tris * 3; i++) {
    auto ind = grid.tris[i] + *offset;
    push(alloc, &ind, 1);
  }
  *offset += grid.num_points;
  *num_inds += grid.num_tris * 3;
}

void push_indices_invert_winding(LinearAllocator* alloc, const TriangulatedGrid& grid,
                                 uint32_t* offset, uint32_t* num_inds) {
  for (uint32_t i = 0; i < grid.num_tris; i++) {
    auto i0 = i * 3;
    auto i1 = i * 3 + 1;
    auto i2 = i * 3 + 2;
    uint32_t inds[3] = {grid.tris[i0], grid.tris[i2], grid.tris[i1]};
    for (auto& ind : inds) {
      ind += *offset;
    }
    push(alloc, inds, 3);
  }
  *offset += grid.num_points;
  *num_inds += grid.num_tris * 3;
}

uint64_t make_grid_cache_key(int w, int h) {
  uint64_t wk{};
  uint64_t hk{};
  memcpy(&wk, &w, sizeof(int));
  memcpy(&hk, &h, sizeof(int));
  wk <<= 32u;
  return wk | hk;
}

TriangulatedGrid cache_entry_to_triangulated_grid(const GridCache* cache,
                                                  const GridCache::Entry& entry) {
  TriangulatedGrid result;
  result.points = cache->points.data() + entry.point_offset;
  result.tris = cache->triangles.data() + entry.tri_offset;
  result.num_points = entry.num_points;
  result.num_tris = entry.num_tris;
  return result;
}

} //  anon

TriangulatedGrid arch::acquire_triangulated_grid(const GridCache* cache, int w, int h) {
  uint64_t k = make_grid_cache_key(w, h);
  assert(cache->entries.count(k) && "First call `require_triangulated_grid`");
  return cache_entry_to_triangulated_grid(cache, cache->entries.at(k));
}

void arch::require_triangulated_grid(GridCache* cache, int w, int h) {
  const uint64_t k = make_grid_cache_key(w, h);
  if (cache->entries.count(k) == 0) {
    auto grid = make_grid<double>(w, h);
    std::vector<cdt::Triangle> tris = cdt::triangulate_simple(grid);
    auto point_offset = uint32_t(cache->points.size());
    auto tri_offset = uint32_t(cache->triangles.size());
    cache->points.insert(cache->points.end(), grid.begin(), grid.end());
    cache->triangles.resize(cache->triangles.size() + tris.size() * 3);
    memcpy(cache->triangles.data() + tri_offset, tris.data(), tris.size() * sizeof(uint32_t) * 3);

    GridCache::Entry entry{};
    entry.tri_offset = tri_offset;
    entry.point_offset = point_offset;
    //  @NOTE, no divide by 3 because `tris` is a vector of triangles.
    entry.num_tris = uint32_t(tris.size());
    entry.num_points = uint32_t(grid.size());
    cache->entries[k] = entry;
  }
}

TriangulatedGrid arch::make_triangulated_grid(const std::vector<cdt::Triangle>& tris,
                                              const std::vector<cdt::Point>& points) {
  TriangulatedGrid result;
  result.points = points.data();
  result.num_points = uint32_t(points.size());
  result.tris = cdt::unsafe_cast_to_uint32(tris.data());
  result.num_tris = uint32_t(tris.size());
  return result;
}

WallHoleResult arch::make_wall_hole(const WallHoleParams& params) {
  auto sseg_ps = make_grid<float>(params.straight_hole_x_segments, params.hole_y_segments);
  auto sseg_ts = triangulate(to_points(sseg_ps));
  auto cseg_ps = make_grid<float>(params.curved_hole_x_segments, params.hole_y_segments);
  auto cseg_ts = triangulate(to_points(cseg_ps));
  auto grid_ps = make_grid<float>(params.grid_x_segments, params.grid_y_segments);
  auto max_num_ps = std::max(grid_ps.size(), std::max(sseg_ps.size(), cseg_ps.size()));

  Context ctx{};
  WallHoleResult result{};
  init_context(&ctx, uint32_t(max_num_ps), params.aspect_ratio);
  for (uint32_t i = 0; i < params.num_holes; i++) {
    add_hole(sseg_ps, sseg_ts, cseg_ps, cseg_ts, params.holes[i], ctx);
  }
  add_background_grid(grid_ps, ctx, result);

  auto tri = triangulate_remove_holes(ctx.pending_tri.points, ctx.tot_cs);
  for (auto& t : tri) {
    ctx.tot_ts.push_back(remap_triangle(t, ctx.tri_to_tot));
  }

  const float inv_aspect = 1.0f / params.aspect_ratio;
  for (auto& p : ctx.tot_ps) {
    p.x *= inv_aspect;
    p = permute(p, params.dim_perm[0], params.dim_perm[1], params.dim_perm[2]);
    p -= Vec3f{0.5f};
  }
  for (auto& n : ctx.tot_ns) {
    n = permute(n, params.dim_perm[0], params.dim_perm[1], params.dim_perm[2]);
  }

  assert(ctx.tot_ps.size() == ctx.tot_ns.size());
  result.triangles = std::move(ctx.tot_ts);
  result.positions = std::move(ctx.tot_ps);
  result.normals = std::move(ctx.tot_ns);
  result.interior_edge_points = std::move(ctx.interior_edge_points);
  return result;
}

TriangulationResult
arch::make_straight_flat_segment(const StraightFlatSegmentParams& params) {
  TriangulationResult result{};
  auto grid_ps = make_grid<double>(params.grid_x_segments, params.grid_y_segments);
  auto grid_ts = triangulate(grid_ps);

  for (auto& p2 : grid_ps) {
    Vec3f p3{float(p2.x), float(p2.y), 0.0f};
    p3 -= Vec3f{0.5f};
    Vec3f n{0.0f, 0.0f, -1.0f};
    p3 = permute(p3, params.dim_perm[0], params.dim_perm[1], params.dim_perm[2]);
    n = permute(n, params.dim_perm[0], params.dim_perm[1], params.dim_perm[2]);
    result.positions.push_back(p3);
    result.normals.push_back(n);
  }

  result.triangles = std::move(grid_ts);
  return result;
}

void arch::make_adjoining_curved_segment(const AdjoiningCurvedSegmentParams& params) {
  uint32_t num_points{};
  uint32_t num_inds{};

  auto* i_beg = params.alloc.tris->begin;
  auto* p_beg = params.alloc.ps->begin;

  for (uint32_t i = 0; i < params.grid.num_points; i++) {
    auto& p = params.grid.points[i];

    if (p.x == 0.0f) {
      if (p.y == 0.0f) {
        params.negative_x->x0_y0 = i + params.index_offset;
      } else if (p.y == 1.0f) {
        params.negative_x->x0_y1 = i + params.index_offset;
      }
    } else if (p.x == 1.0f) {
      if (p.y == 0.0f) {
        params.positive_x->x0_y0 = i + params.index_offset;
      } else if (p.y == 1.0f) {
        params.positive_x->x0_y1 = i + params.index_offset;
      }
    }

    auto [p2, n2] = adjoining_curved_segment(
      params.p0, params.p1, params.v0, params.v1, params.n0, params.n1, float(p.x));
    Vec3f p3{p2.x, float(p.y) * params.y_scale + params.y_offset, p2.y};
    Vec3f n{n2.x, 0.0f, n2.y};
    push(params.alloc.ps, &p3, 1);
    push(params.alloc.ns, &n, 1);
    num_points++;
  }
  uint32_t ind_off = params.index_offset;
  push_indices(params.alloc.tris, params.grid, &ind_off, &num_inds);

#if 1
  tri::require_ccw(i_beg, num_inds/3, p_beg, sizeof(Vec3f), 0, params.index_offset);
#endif

  *params.num_points_added = num_points;
  *params.num_indices_added = num_inds;
}

void arch::make_curved_vertical_connection(const CurvedVerticalConnectionParams& params) {
  //  sides
  uint32_t ind_off = params.index_offset;
  uint32_t num_side_points{};
  uint32_t num_indices_added{};
  auto* p_beg = params.alloc.ps->p;
  auto* n_beg = params.alloc.ns->p;
  auto* i_beg = params.alloc.tris->p;
  for (int iter = 0; iter < 2; iter++) {
    for (uint32_t i = 0; i < params.xy.num_points; i++) {
      auto [x, y] = curved_vertical_connection(params.xy.points[i], params);
      float z = iter == 0 ? 0.0f : 1.0f;
      float n_sign = iter == 0 ? -1.0f : 1.0f;
      Vec3f p{x, y, z};
      Vec3f n{0.0f, 0.0f, n_sign};
      push(params.alloc.ps, &p, 1);
      push(params.alloc.ns, &n, 1);
      num_side_points++;
    }
    push_indices(params.alloc.tris, params.xy, &ind_off, &num_indices_added);
  }
  //  top
  uint32_t num_top_points{};
  for (uint32_t i = 0; i < params.xz.num_points; i++) {
    Vec2<double> eval_p{params.xz.points[i].x, 1.0};
    auto [x, y] = curved_vertical_connection(eval_p, params);
    Vec3f p{x, y, 1.0f - float(params.xz.points[i].y)};
    push(params.alloc.ps, &p, 1);
    push(params.alloc.ns, &p, 1); //  @NOTE
    num_top_points++;
  }
  push_indices(params.alloc.tris, params.xz, &ind_off, &num_indices_added);
  //  transform
  auto* cts = allocate_n<uint32_t>(params.alloc.tmp, num_top_points);
  zero_memory_n<uint32_t>(cts, num_top_points);
  tri::compute_normals(
    params.xz.tris,
    params.xz.num_tris,
    p_beg + num_side_points * sizeof(Vec3f),
    n_beg + num_side_points * sizeof(Vec3f),
    cts);
  transform_positions_to_obb(
    p_beg,
    num_side_points + num_top_points,
    params.bounds,
    Vec3f{2.0f}, Vec3f{1.0f});
  transform_normals_to_obb(
    n_beg,
    num_side_points + num_top_points,
    params.bounds);

#if 1
  tri::require_ccw(
    i_beg, num_indices_added/3, p_beg, sizeof(Vec3f), 0, params.index_offset);
#endif

  *params.num_indices_added = num_indices_added;
  *params.num_points_added = num_side_points + num_top_points;
}

WallParams arch::make_wall_params(const OBB3f& wall_bounds,
                                  uint32_t base_index_offset,
                                  const WallHoleResult& hole_res,
                                  const TriangulationResult& seg_res,
                                  GeometryAllocators alloc,
                                  uint32_t* num_points_added,
                                  uint32_t* num_indices_added,
                                  FaceConnectorIndices* positive_x,
                                  FaceConnectorIndices* negative_x) {
  arch::WallParams wall_p{};
  wall_p.bounds = wall_bounds;
  wall_p.base_index_offset = base_index_offset;
  wall_p.wall_ps = hole_res.positions.data();
  wall_p.wall_ns = hole_res.normals.data();
  wall_p.num_wall_points = uint32_t(hole_res.positions.size());
  wall_p.wall_tris = cdt::unsafe_cast_to_uint32(hole_res.triangles.data());
  wall_p.num_wall_tris = uint32_t(hole_res.triangles.size());
  wall_p.wall_interior_inds = &hole_res.interior_edge_points;
  wall_p.wall_bot_l_ind = hole_res.bot_l_ind;
  wall_p.wall_bot_r_ind = hole_res.bot_r_ind;
  wall_p.wall_top_r_ind = hole_res.top_r_ind;
  wall_p.wall_top_l_ind = hole_res.top_l_ind;
  wall_p.flat_ps = seg_res.positions.data();
  wall_p.flat_ns = seg_res.normals.data();
  wall_p.num_flat_points = uint32_t(seg_res.positions.size());
  wall_p.flat_tris = cdt::unsafe_cast_to_uint32(seg_res.triangles.data());
  wall_p.num_flat_tris = uint32_t(seg_res.triangles.size());
  wall_p.alloc = alloc;
  wall_p.num_points_added = num_points_added;
  wall_p.num_indices_added = num_indices_added;
  wall_p.positive_x = positive_x;
  wall_p.negative_x = negative_x;
  return wall_p;
}

void arch::make_wall(const WallParams& params) {
  constexpr Vec3f wall_face_size_scale{2.0f, 2.0f, 1.0f};
  constexpr Vec3f wall_face_offset_scale{0.0f, 0.0f, 0.5f};

  const uint32_t orig_num_wall_points = params.num_wall_points;
  const uint32_t orig_num_wall_inds = params.num_wall_tris * 3;
  const uint32_t base_ind_off = params.base_index_offset;

  auto* const ps_beg = params.alloc.ps->p;
  auto* const tris_beg = params.alloc.tris->p;

  auto* const tot_ps = allocate_n<Vec3f>(params.alloc.ps, orig_num_wall_points * 2);
  auto* const tot_ns = allocate_n<Vec3f>(params.alloc.ns, orig_num_wall_points * 2);
  auto* const tot_tris = allocate_n<uint32_t>(params.alloc.tris, orig_num_wall_inds * 2);

  {
    FaceConnectorIndices positive_x{};
    positive_x.x0_y0 = params.wall_bot_r_ind + base_ind_off;
    positive_x.x0_y1 = params.wall_top_r_ind + base_ind_off;
    positive_x.x1_y0 = params.wall_bot_r_ind + base_ind_off + orig_num_wall_points;
    positive_x.x1_y1 = params.wall_top_r_ind + base_ind_off + orig_num_wall_points;
    *params.positive_x = positive_x;
  }
  {
    FaceConnectorIndices negative_x{};
    negative_x.x0_y0 = params.wall_bot_l_ind + base_ind_off;
    negative_x.x0_y1 = params.wall_top_l_ind + base_ind_off;
    negative_x.x1_y0 = params.wall_bot_l_ind + base_ind_off + orig_num_wall_points;
    negative_x.x1_y1 = params.wall_top_l_ind + base_ind_off + orig_num_wall_points;
    *params.negative_x = negative_x;
  }

  uint32_t num_points_added{};
  uint32_t num_inds_added{};

  const auto sv3 = sizeof(Vec3f);
  const auto su32 = sizeof(uint32_t);
  for (int i = 0; i < 2; i++) {
    auto off_p = orig_num_wall_points * i;
    auto off_i = orig_num_wall_inds * i;
    memcpy(tot_ps + off_p * sv3, params.wall_ps, orig_num_wall_points * sv3);
    memcpy(tot_ns + off_p * sv3, params.wall_ns, orig_num_wall_points * sv3);
    memcpy(tot_tris + off_i * su32, params.wall_tris, orig_num_wall_inds * su32);
    num_points_added += orig_num_wall_points;
  }

  for (size_t i = 0; i < orig_num_wall_inds; i++) {
    uint32_t ind;
    read_ith(&ind, tot_tris, i);
    ind += base_ind_off;
    write_ith(tot_tris, &ind, i);
    num_inds_added++;
  }
  for (size_t i = orig_num_wall_inds; i < orig_num_wall_inds * 2; i++) {
    uint32_t ind;
    read_ith(&ind, tot_tris, i - orig_num_wall_inds);
    ind += orig_num_wall_points;
    write_ith(tot_tris, &ind, i);
    num_inds_added++;
  }

  //  First wall face
  transform_positions_to_obb(
    tot_ps, orig_num_wall_points, params.bounds, wall_face_size_scale, wall_face_offset_scale);
  transform_normals_to_obb(tot_ns, orig_num_wall_points, params.bounds);
  //  Opposite wall face
  transform_positions_to_obb(
    tot_ps + orig_num_wall_points * sv3,
    orig_num_wall_points,
    negate_k(params.bounds),
    wall_face_size_scale,
    wall_face_offset_scale);
  transform_normals_to_obb(
    tot_ns + orig_num_wall_points * sv3, orig_num_wall_points, negate_k(params.bounds));
  //  Use same points for interior edges
  if (params.wall_interior_inds) {
    for (size_t i = orig_num_wall_inds; i < orig_num_wall_inds * 2; i++) {
      uint32_t ind;
      read_ith(&ind, tot_tris, i);
      auto i0 = uint32_t(ind - orig_num_wall_points - base_ind_off);
      if (params.wall_interior_inds->count(i0)) {
        i0 += base_ind_off;
        write_ith(tot_tris, &i0, i);
      }
    }
  }

  //  side segments
  std::array<std::array<int, 3>, 4> dim_perms{};
  dim_perms[0] = {{0, 1, 2}};
  dim_perms[1] = {{0, 1, 2}};
  dim_perms[2] = {{1, 0, 2}};
  dim_perms[3] = {{1, 0, 2}};
  std::array<Vec3f, 4> norm_signs{{
    Vec3f{1.0f},
    Vec3f{-1.0f},
    Vec3f{1.0f},
    Vec3f{-1.0f}
  }};
  std::array<Vec3f, 4> scale_offs{{
    Vec3f{},
    Vec3f{-2.0f, 0.0f, 0.0f},
    Vec3f{},
    Vec3f{0.0f, -2.0f, 0.0f}
  }};
  for (int iter = 0; iter < 4; iter++) {
    auto* dst_ps = allocate_n<Vec3f>(params.alloc.ps, params.num_flat_points);
    auto* dst_ns = allocate_n<Vec3f>(params.alloc.ns, params.num_flat_points);
    auto* dst_i = allocate_n<uint32_t>(params.alloc.tris, params.num_flat_tris * 3);

    for (uint32_t i = 0; i < params.num_flat_tris * 3; i++) {
      uint32_t ind = params.flat_tris[i] + num_points_added + base_ind_off;
      write_ith(dst_i, &ind, i);
      num_inds_added++;
    }

    const auto& dp = dim_perms[iter];
    const auto& scale_off = scale_offs[iter];
    const auto& norm_sign = norm_signs[iter];
    for (uint32_t pi = 0; pi < params.num_flat_points; pi++) {
      auto p = permute(params.flat_ps[pi], dp[0], dp[1], dp[2]);
      memcpy(dst_ps + pi * sv3, &p, sv3);

      auto use_n = params.flat_ns[pi] * norm_sign;
      use_n = permute(use_n, dp[0], dp[1], dp[2]);
      memcpy(dst_ns + pi * sv3, &use_n, sv3);
      num_points_added++;
    }

    transform_positions_to_obb(
      dst_ps, params.num_flat_points, params.bounds, Vec3f{2.0f}, scale_off);
    transform_normals_to_obb(dst_ns, params.num_flat_points, params.bounds);
  }

#if 1
  tri::require_ccw(
    tris_beg, num_inds_added/3, ps_beg, sizeof(Vec3f), 0, base_ind_off);
#endif

  *params.num_points_added = num_points_added;
  *params.num_indices_added = num_inds_added;
}

void arch::make_arch_wall(const ArchWallParams& params) {
  //  top arch
  auto* p_beg = params.alloc.ps->p;
  auto* n_beg = params.alloc.ns->p;
  auto* i_beg = params.alloc.tris->p;
  const uint32_t base_index_offset = params.index_offset;

  uint32_t index_offset = base_index_offset;
  uint32_t num_points{};
  uint32_t num_indices{};
  for (uint32_t i = 0; i < params.arch_xz.num_points; i++) {
    auto& p = params.arch_xz.points[i];
    auto pt = frac_radial_point(float(p.y), params.outer_radius);
    Vec3f dst_p{float(p.x) * params.width, pt.x, pt.y};
    push(params.alloc.ps, &dst_p, 1);
    push(params.alloc.ns, &dst_p, 1); //  @TODO
    num_points++;
  }
  push_indices_invert_winding(params.alloc.tris, params.arch_xz, &index_offset, &num_indices);

  //  bottom arch
  const float addtl_side_width = params.side_additional_width;
  const float addtl_width_pow = params.side_additional_width_power;
  for (uint32_t i = 0; i < params.arch_xz.num_points; i++) {
    auto& p = params.arch_xz.points[i];
    auto pb = frac_radial_point(float(p.y), params.inner_radius);
    float x0 = -addtl_side_width;
    float x1 = params.width + addtl_side_width;
    float x = lerp(float(p.x), x0, x1);
    Vec3f dst_p{x, pb.x, pb.y};
    push(params.alloc.ps, &dst_p, 1);
    push(params.alloc.ns, &dst_p, 1); //  @TODO
    num_points++;
  }
  push_indices(params.alloc.tris, params.arch_xz, &index_offset, &num_indices);

  //  side arch
  for (uint32_t iter = 0; iter < 2; iter++) {
    for (uint32_t i = 0; i < params.arch_yz.num_points; i++) {
      auto& p = params.arch_yz.points[i];
      float frac_side_p = 1.0f - float(p.x);
      auto frac_top_p = float(p.y);

      auto p_top = frac_radial_point(frac_top_p, params.outer_radius);
      auto p_bot = frac_radial_point(frac_top_p, params.inner_radius);
      auto p_tmp = lerp(frac_side_p, p_top, p_bot);
      auto off_x = std::pow(frac_side_p, addtl_width_pow) * addtl_side_width;
      Vec3f v;
      if (iter == 0) {
        v = Vec3f{-off_x, p_tmp.x, p_tmp.y};
      } else {
        v = Vec3f{off_x + params.width, p_tmp.x, p_tmp.y};
      }
      push(params.alloc.ps, &v, 1);
      push(params.alloc.ns, &v, 1); //  @TODO
      num_points++;
    }
    if (iter == 0) {
      push_indices_invert_winding(
        params.alloc.tris, params.arch_yz, &index_offset, &num_indices);
    } else {
      push_indices(params.alloc.tris, params.arch_yz, &index_offset, &num_indices);
    }
  }

  //  side yz
  for (uint32_t iter = 0; iter < 4; iter++) {
    const float z_sign = iter == 0 || iter == 1 ? 1.0f : -1.0f;
    const float x_sign = iter == 0 || iter == 2 ? 1.0f : -1.0f;
    for (uint32_t i = 0; i < params.straight_yz.num_points; i++) {
      auto [x, y] = params.straight_yz.points[i];
      auto off_x = std::pow(float(x), addtl_width_pow) * addtl_side_width;
      off_x += params.width;
      const float z = lerp(float(x), params.outer_radius, params.inner_radius);
      Vec3f v;
      float y_use = float(y) * params.straight_length_scale;
      if (iter == 0 || iter == 2) {
        v = Vec3f{off_x, y_use, z * z_sign};
      } else {
        v = Vec3f{-off_x + params.width, y_use, z * z_sign};
      }
      push(params.alloc.ps, &v, 1);
      push(params.alloc.ns, &v, 1); //  @TODO
      num_points++;
    }
    if (z_sign * x_sign == -1.0f) {
      push_indices_invert_winding(
        params.alloc.tris, params.straight_yz, &index_offset, &num_indices);
    } else {
      push_indices(params.alloc.tris, params.straight_yz, &index_offset, &num_indices);
    }
  }

  //  side xz
  for (uint64_t iter = 0; iter < 4; iter++) {
    const float z_sign = (iter % 2 == 0) ? 1.0f : -1.0f;
    const float i_sign = iter < 2 ? 1.0f : -1.0f;
    for (uint32_t i = 0; i < params.straight_xz.num_points; i++) {
      auto& p = params.straight_xz.points[i];
      float x0 = iter < 2 ? 0.0f : -addtl_side_width;
      float x1 = iter < 2 ? params.width : params.width + addtl_side_width;
      const float target_radius = iter < 2 ? params.outer_radius : params.inner_radius;

      const auto x = float(p.x);
      const auto y = float(p.y);
      float z_use = z_sign * target_radius;
      float x_use = lerp(x, x0, x1);

      Vec3f v{x_use, y * params.straight_length_scale, z_use};
      push(params.alloc.ps, &v, 1);
      push(params.alloc.ns, &v, 1); //  @TODO
      num_points++;
    }
    if (z_sign * i_sign == -1.0f) {
      push_indices_invert_winding(
        params.alloc.tris, params.straight_xz, &index_offset, &num_indices);
    } else {
      push_indices(params.alloc.tris, params.straight_xz, &index_offset, &num_indices);
    }
  }

  const auto num_tris = (params.alloc.tris->p - i_beg) / sizeof(uint32_t) / 3;
  auto* cts = allocate_n<uint32_t>(params.alloc.tmp, num_points);
  zero_memory_n<uint32_t>(cts, num_points);

  const bool invert[3] = {false, true, false};
  const int permute[3] = {2, 1, 0};
  normalize_vec3_to_01(p_beg, num_points, invert, permute);
  transform_positions_to_obb(p_beg, num_points, params.bounds, Vec3f{2.0f}, Vec3f{1.0f});
  tri::compute_normals(i_beg, uint32_t(num_tris), p_beg, n_beg, cts, params.index_offset);

#if 1
  tri::require_ccw(i_beg, num_indices/3, p_beg, sizeof(Vec3f), 0, base_index_offset);
#endif

  *params.num_points_added = num_points;
  *params.num_indices_added = num_indices;
}

void arch::make_pole(const PoleParams& params) {
  uint32_t num_points_added{};
  uint32_t num_inds_added{};
  uint32_t ind_off = params.index_offset;
  auto* p_beg = params.alloc.ps->p;
  auto* n_beg = params.alloc.ns->p;
  for (uint32_t i = 0; i < params.grid.num_points; i++) {
    auto& p = params.grid.points[i];
    const auto p_xz = frac_radial_point(float(p.x) * 2.0f, 1.0f);

    Vec3f p3{p_xz.x * 0.5f + 0.5f, float(p.y), p_xz.y * 0.5f + 0.5f};
    Vec3f n{p_xz.x, 0.0f, p_xz.y};

    push(params.alloc.ps, &p3, 1);
    push(params.alloc.ns, &n, 1);
    num_points_added++;
  }
  push_indices(params.alloc.tris, params.grid, &ind_off, &num_inds_added);
  transform_positions_to_obb(
    p_beg,
    num_points_added,
    params.bounds,
    Vec3f{2.0f},
    Vec3f{1.0f});
  transform_normals_to_obb(n_beg, num_points_added, params.bounds);
  *params.num_points_added = num_points_added;
  *params.num_indices_added = num_inds_added;
}

GeometryAllocators
arch::make_geometry_allocators(LinearAllocator* ps, LinearAllocator* ns,
                               LinearAllocator* tris, LinearAllocator* tmp) {
  return {ps, ns, tris, tmp};
}

void arch::clear_geometry_allocators(const GeometryAllocators* alloc) {
  clear(alloc->ps);
  clear(alloc->ns);
  clear(alloc->tris);
  clear(alloc->tmp);
}

void arch::truncate_to_uint16(const uint32_t* src, uint16_t* dst, size_t num_indices) {
  for (size_t i = 0; i < num_indices; i++) {
    assert(src[i] < (1u << 16u));
    dst[i] = uint16_t(src[i]);
  }
}

void arch::WallHole::push_default3(WallHole* result) {
  auto& hole0 = result[0];
  hole0.scale = Vec2f{0.25f};
  hole0.curl = 0.4f;
  hole0.rot = 0.1f;
  hole0.off = Vec2f{0.1f, -0.1f};

  auto& hole1 = result[1];
  hole1.scale = Vec2f{0.25f, 0.3f};
  hole1.curl = 0.2f;
  hole1.rot = -0.3f;
  hole1.off = Vec2f{-0.2f, 0.2f};

  auto& hole2 = result[2];
  hole2.scale = Vec2f{0.1f, 0.2f};
  hole2.curl = 0.2f;
  hole2.rot = 0.3f;
  hole2.off = Vec2f{0.3f, 0.3f};
}

GROVE_NAMESPACE_END
