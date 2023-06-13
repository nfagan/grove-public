#include "render.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include "grove/common/Optional.hpp"
#include "grove/math/triangle.hpp"
#include "grove/math/util.hpp"
#include "grove/math/ease.hpp"
#include "grove/math/random.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace arch;

RenderTriangleGrowthData
make_render_triangle_growth_data(const uint32_t* src_tris, uint32_t num_src_tris,
                                 const void* src_p, uint32_t src_stride, uint32_t src_offset,
                                 void* dst_p, uint32_t dst_stride, uint32_t dst_offset) {
  RenderTriangleGrowthData result{};
  result.src_tris = src_tris;
  result.num_src_tris = num_src_tris;
  result.src_p = src_p;
  result.src_stride = src_stride;
  result.src_offset = src_offset;
  result.dst_p = dst_p;
  result.dst_stride = dst_stride;
  result.dst_offset = dst_offset;
  return result;
}

const uint32_t* ith_tri(const RenderTriangleGrowthData& data, uint32_t i) {
  return data.src_tris + i * 3;
}

uint32_t find_adjacent(const RenderTriangleGrowthData& data,
                       uint32_t ti, uint32_t ai, uint32_t bi) {
  return tri::find_adjacent_order_independent(data.src_tris, data.num_src_tris, ti, ai, bi);
}

std::vector<uint32_t>
find_non_visited_tis_with_pi(const RenderTriangleRecedeContext& context, uint32_t pi) {
  std::vector<uint32_t> result;
  for (uint32_t i = 0; i < context.data.num_src_tris; i++) {
    const auto* tri = ith_tri(context.data, i);
    if (tri::contains_point(tri, pi) && context.visited_ti.count(i) == 0) {
      result.push_back(i);
    }
  }
  return result;
}

int ccw_index(int i) {
  return (i + 1) % 3;
}

int find_index(const uint32_t* tri, uint32_t ind) {
  for (int i = 0; i < 3; i++) {
    if (tri[i] == ind) {
      return i;
    }
  }
  return -1;
}

Vec3f read_point(const void* data, uint32_t pi, uint32_t stride, uint32_t offset) {
  if (!stride) {
    stride = sizeof(Vec3f);
  }
  auto* read = static_cast<const unsigned char*>(data) + stride * pi + offset;
  Vec3f res;
  memcpy(&res, read, sizeof(Vec3f));
  return res;
}

void write_point(void* data, const Vec3f& p, uint32_t pi, uint32_t stride, uint32_t offset) {
  if (!stride) {
    stride = sizeof(Vec3f);
  }
  auto* write = static_cast<unsigned char*>(data) + stride * pi + offset;
  memcpy(write, &p, sizeof(Vec3f));
}

Vec3f read_src_point(const RenderTriangleGrowthData& data, uint32_t pi) {
  return read_point(data.src_p, pi, data.src_stride, data.src_offset);
}

void write_dst_point(const RenderTriangleGrowthData& data, uint32_t pi, const Vec3f& p) {
  write_point(data.dst_p, p, pi, data.dst_stride, data.dst_offset);
}

GrowingTriangle make_receding(const RenderTriangleGrowthData& data, uint32_t ti, uint32_t pi) {
  const uint32_t* tri = ith_tri(data, ti);
  GrowingTriangle receding{};
  receding.src_ti = ti;
  tri::setdiff_point(tri, pi, &receding.src_edge_pi0, &receding.src_edge_pi1);
  return receding;
}

RecedingTriangleSet make_receding_set(const RenderTriangleGrowthData& data,
                                      const std::vector<uint32_t>& tis_with_pi,
                                      uint32_t target_pi, float incr_randomness_range) {
  RecedingTriangleSet receding_set{};
  receding_set.incr_scale = std::max(0.0f, 1.0f + urand_11f() * incr_randomness_range);
  receding_set.receding.resize(tis_with_pi.size());
  int ti_ind{};
  for (uint32_t ti : tis_with_pi) {
    receding_set.receding[ti_ind++] = make_receding(data, ti, target_pi);
  }
  return receding_set;
}

} //  anon

/*
 * . find the first unvisited pending triangle and mark it as visited. if there are none, return.
 * . choose an invoking edge from the triangle and insert it into a growing set.
 * . for every triangle in the growing set:
 *   . find the target point that is not part of the invoking edge.
 *   . form an edge between this point and the ccw point, which must be on the invoking edge.
 *   . grow the edge towards the target point.
 * . when context.f == 1.0:
 *   . for every triangle in the growing set:
 *     . for every edge in the triangle:
 *       . find adjacent triangle to the edge. if this triangle has been visited, continue.
 *       . mark the triangle as visited, and add it to a new growing set along with the new invoking
 *         edge.
 */

bool arch::tick_triangle_growth(RenderTriangleGrowthContext& context,
                                const RenderTriangleGrowthParams& params) {
  if (context.growing.empty()) {
    while (!context.pending_ti.empty()) {
      auto pend_ti = context.pending_ti.back();
      context.pending_ti.pop_back();
      if (context.visited_ti.count(pend_ti)) {
        continue;
      }
      context.visited_ti.insert(pend_ti);
      const auto* tri = ith_tri(context.data, pend_ti);
      uint32_t invoke_src_pi0 = tri[0];
      uint32_t invoke_src_pi1 = tri[1];
      GrowingTriangle growing{};
      growing.src_ti = pend_ti;
      growing.src_edge_pi0 = invoke_src_pi0;
      growing.src_edge_pi1 = invoke_src_pi1;
      context.growing.push_back(growing);

      auto src_pa = read_src_point(context.data, invoke_src_pi0);
      auto src_pb = read_src_point(context.data, invoke_src_pi1);
      auto src_p = (src_pb - src_pa) * 0.5f + src_pa;
      write_dst_point(context.data, pend_ti * 3 + 2, src_p);
      break;
    }
    if (context.growing.empty()) {
      return false;
    }
  }

  assert(!context.growing.empty());
  context.f = clamp(context.f + params.incr, 0.0f, 1.0f);
//  const float t = ease::in_out_quad(context.f);
  const float t = context.f;
  for (auto& grow : context.growing) {
    const uint32_t src_ti = grow.src_ti;
    const uint32_t* src_tri = ith_tri(context.data, src_ti);
    auto pi_targ = tri::setdiff_edge(src_tri, grow.src_edge_pi0, grow.src_edge_pi1);
    const int pi_ind = find_index(src_tri, pi_targ);
    assert(pi_ind >= 0);

    auto p0 = read_src_point(context.data, grow.src_edge_pi0);
    auto p2 = read_src_point(context.data, grow.src_edge_pi1);
    p0 = (p2 - p0) * 0.5f + p0;

    auto p1 = read_src_point(context.data, pi_targ);
    auto p = lerp(t, p0, p1);
    write_dst_point(context.data, src_ti * 3 + pi_ind, p);
  }
  if (context.f == 1.0f) {
    context.f = 0.0f;
    std::vector<GrowingTriangle> next_growing;
    for (auto& grow : context.growing) {
      const uint32_t src_ti = grow.src_ti;
      const uint32_t* src_tri = ith_tri(context.data, src_ti);
      for (int i = 0; i < 3; i++) {
        uint32_t ai = src_tri[i];
        uint32_t bi = src_tri[ccw_index(i)];
        uint32_t adj_ti = find_adjacent(context.data, src_ti, ai, bi);
        if (adj_ti != tri::no_adjacent_triangle() && context.visited_ti.count(adj_ti) == 0) {
          context.visited_ti.insert(adj_ti);
          const uint32_t* adj_tri = ith_tri(context.data, adj_ti);
          uint32_t next_targ_pi = tri::setdiff_edge(adj_tri, ai, bi);
          const int next_targ_pi_ind = find_index(adj_tri, next_targ_pi);
          assert(next_targ_pi_ind >= 0);

          GrowingTriangle next_grow{};
          next_grow.src_ti = adj_ti;
          next_grow.src_edge_pi0 = ai;
          next_grow.src_edge_pi1 = bi;
          next_growing.push_back(next_grow);

          auto src_pa = read_src_point(context.data, ai);
          auto src_pb = read_src_point(context.data, bi);
          auto src_p = (src_pb - src_pa) * 0.5f + src_pa;
          write_dst_point(context.data, adj_ti * 3 + next_targ_pi_ind, src_p);
        }
      }
    }
    context.growing = std::move(next_growing);
  }
  return true;
}

uint32_t arch::tick_triangle_growth(RenderTriangleGrowthContext* ctx, uint16_t* dst_inds,
                                    uint32_t max_num_inds, float growth_incr) {
  (void) max_num_inds;
  arch::RenderTriangleGrowthParams params{};
  params.incr = growth_incr;
  if (arch::tick_triangle_growth(*ctx, params)) {
    uint32_t curr_ind{};
    for (uint32_t visited_ti : ctx->visited_ti) {
      for (int i = 0; i < 3; i++) {
        const uint32_t dst_ind = visited_ti * 3 + i;
        assert(dst_ind < (1u << 16u) && dst_ind < max_num_inds);
        dst_inds[curr_ind++] = uint16_t(dst_ind);
      }
    }
    return curr_ind;
  } else {
    return 0;
  }
}

void arch::initialize_triangle_growth(RenderTriangleGrowthContext* ctx,
                                      const uint32_t* src_tris, uint32_t num_src_tris,
                                      const void* src_p, uint32_t src_stride, uint32_t src_offset,
                                      void* dst_p, uint32_t dst_stride, uint32_t dst_offset) {
  *ctx = {};
  ctx->data = make_render_triangle_growth_data(
    src_tris, num_src_tris,
    src_p, src_stride, src_offset,
    dst_p, dst_stride, dst_offset);
  for (uint32_t i = 0; i < num_src_tris; i++) {
    ctx->pending_ti.push_back(i);
  }
}

bool arch::tick_triangle_recede(RenderTriangleRecedeContext& context,
                                const RenderTriangleRecedeParams& params) {
  const int num_targ_sets = params.num_target_sets;
  while (!context.pending_pi.empty() &&
        (num_targ_sets < 0 || int(context.receding_sets.size()) < num_targ_sets)) {
    const uint32_t candidate_pi = context.pending_pi.back();
    context.pending_pi.pop_back();

    if (context.visited_pi.count(candidate_pi)) {
      continue;
    } else {
      context.visited_pi.insert(candidate_pi);
    }

    auto tis_with_pi = find_non_visited_tis_with_pi(context, candidate_pi);
    if (tis_with_pi.empty()) {
      continue;
    } else {
      for (uint32_t ti : tis_with_pi) {
        context.visited_ti.insert(ti);
      }
    }

    context.receding_sets.push_back(make_receding_set(
      context.data, tis_with_pi, candidate_pi, params.incr_randomness_range));
  }

  if (context.receding_sets.empty()) {
    return false;
  }

  assert(!context.receding_sets.empty());
  auto recede_it = context.receding_sets.begin();
  while (recede_it != context.receding_sets.end()) {
    auto& recede_set = *recede_it;
    recede_set.f = clamp(recede_set.f + params.incr * recede_set.incr_scale, 0.0f, 1.0f);
    const float t = recede_set.f;

    for (auto& recede_tri : recede_set.receding) {
      const uint32_t* tri = ith_tri(context.data, recede_tri.src_ti);
      const uint32_t edge_pi0 = recede_tri.src_edge_pi0;
      const uint32_t edge_pi1 = recede_tri.src_edge_pi1;
      const uint32_t targ_pi = tri::setdiff_edge(tri, edge_pi0, edge_pi1);

      const int edge_pi0_ind = find_index(tri, edge_pi0);
      const int edge_pi1_ind = find_index(tri, edge_pi1);
      assert(edge_pi0_ind >= 0 && edge_pi1_ind >= 0);

      const Vec3f targ_p = read_src_point(context.data, targ_pi);
      const Vec3f edge_p0 = read_src_point(context.data, edge_pi0);
      const Vec3f edge_p1 = read_src_point(context.data, edge_pi1);

      auto dst_p0 = lerp(t, edge_p0, targ_p);
      auto dst_p1 = lerp(t, edge_p1, targ_p);

      const uint32_t dst_pi0 = recede_tri.src_ti * 3 + edge_pi0_ind;
      const uint32_t dst_pi1 = recede_tri.src_ti * 3 + edge_pi1_ind;
      write_dst_point(context.data, dst_pi0, dst_p0);
      write_dst_point(context.data, dst_pi1, dst_p1);
    }

    if (t == 1.0f) {
      recede_it = context.receding_sets.erase(recede_it);
    } else {
      ++recede_it;
    }
  }

  return true;
}

void arch::initialize_triangle_recede(RenderTriangleRecedeContext* ctx,
                                      const uint32_t* src_tris, uint32_t num_src_tris,
                                      const void* src_p, uint32_t src_stride, uint32_t src_offset,
                                      void* dst_p, uint32_t dst_stride, uint32_t dst_offset) {
  *ctx = {};

  ctx->data = make_render_triangle_growth_data(
    src_tris, num_src_tris,
    src_p, src_stride, src_offset,
    dst_p, dst_stride, dst_offset);

  std::unordered_set<uint32_t> unique_pi;
  for (uint32_t i = 0; i < num_src_tris * 3; i++) {
    unique_pi.insert(src_tris[i]);
  }

  for (uint32_t pi : unique_pi) {
    ctx->pending_pi.push_back(pi);
  }
}

GROVE_NAMESPACE_END
