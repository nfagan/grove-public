#include "ray_project_adjacency.hpp"
#include "grove/math/triangle_search.hpp"
#include "grove/math/intersect.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace ray_project;

using Edge = NonAdjacentConnections::Edge;
using Key = NonAdjacentConnections::Key;
using Entry = NonAdjacentConnections::Entry;

Edge sort_edge(Edge e) {
  if (e.i1 < e.i0) {
    std::swap(e.i0, e.i1);
  }
  return e;
}

bool equal_edge(const Edge& a, const Edge& b) {
  return (a.i0 == b.i0 && a.i1 == b.i1) || (a.i0 == b.i1 && a.i1 == b.i0);
}

bool less_edge(const Edge& a, const Edge& b) {
  auto ap = sort_edge(a);
  auto bp = sort_edge(b);
  return ap.i0 < bp.i0 || (ap.i0 == bp.i0 && ap.i1 < bp.i1);
}

bool less_key(const Key& a, const Key& b) {
  return a.ti < b.ti || (a.ti == b.ti && less_edge(a.edge, b.edge));
}

bool equal_key(const Key& a, const Key& b) {
  return a.ti == b.ti && equal_edge(a.edge, b.edge);
}

bool less_src_entry(const Entry& a, const Entry& b) {
  return less_key(a.src, b.src);
}

Entry make_query(const Key& key) {
  Entry result{};
  result.src = key;
  return result;
}

} //  anon

NonAdjacentConnections::Key
ray_project::make_non_adjacent_connection_key(uint32_t ti, uint32_t pia, uint32_t pib) {
  NonAdjacentConnections::Key result{};
  result.ti = ti;
  result.edge = {pia, pib};
  return result;
}

NonAdjacentConnections::Entry
ray_project::make_non_adjacent_connection_entry(const NonAdjacentConnections::Key& src,
                                                const NonAdjacentConnections::Key& target,
                                                const Vec2f& edge_coords) {
  NonAdjacentConnections::Entry result{};
  result.src = src;
  result.target = target;
  result.target_edge_fractional_coordinates = edge_coords;
  return result;
}

void ray_project::push_pending_non_adjacent_connection(NonAdjacentConnections* connections,
                                                       const NonAdjacentConnections::Entry& entry) {
  connections->entries.push_back(entry);
  connections->need_build = true;
}

void ray_project::build_non_adjacent_connections(NonAdjacentConnections* connections) {
  auto& entries = connections->entries;
  std::sort(entries.begin(), entries.end(), less_src_entry);
  connections->need_build = false;
}

NonAdjacentConnections::Iterator
ray_project::find_non_adjacent_connections(const NonAdjacentConnections* connections,
                                           const NonAdjacentConnections::Key& src_key) {
  auto& entries = connections->entries;
  assert(!connections->need_build && std::is_sorted(entries.begin(), entries.end(), less_src_entry));
  auto it = std::lower_bound(entries.begin(), entries.end(), make_query(src_key), less_src_entry);

  NonAdjacentConnections::Iterator result{};
  result.begin = (it - entries.begin()) + entries.data();
  while (it != entries.end() && equal_key(it->src, src_key)) {
    ++it;
  }
  result.end = (it - entries.begin()) + entries.data();
  return result;
}

void ray_project::push_axis_aligned_non_adjacent_connections(NonAdjacentConnections* connections,
                                                             const uint32_t* i0, uint32_t i0_size,
                                                             const uint32_t* i1, uint32_t i1_size,
                                                             const tri::EdgeToIndex<uint32_t>& edge_indices,
                                                             const void* vertices,
                                                             uint32_t stride, uint32_t p_off,
                                                             float tol, int axis) {
  assert(axis >= 0 && axis < 3);
  assert(i0_size % 2 == 0 && i1_size % 2 == 0);

  const auto* verts = static_cast<const unsigned char*>(vertices);
  for (uint32_t i = 0; i < i0_size/2; i++) {
    uint32_t src0 = i0[i * 2];
    uint32_t src1 = i0[i * 2 + 1];
    auto src_inds = tri::find_ti_with_edge(edge_indices, src0, src1);
    if (src_inds.num_tis != 1) {
      //  Only try to connect if there is exactly one triangle with this edge. If there is more
      //  than one triangle with this edge, then it is already properly connected.
      continue;
    }

    const uint32_t src_ti = src_inds.tis[0];
    Vec3f src_p0;
    Vec3f src_p1;
    memcpy(&src_p0, verts + stride * src0 + p_off, sizeof(Vec3f));
    memcpy(&src_p1, verts + stride * src1 + p_off, sizeof(Vec3f));

    if (src_p1[axis] < src_p0[axis]) {
      std::swap(src0, src1);
      std::swap(src_p0, src_p1);
    }

    const Vec2f check_src[2] = {
      exclude(src_p0, axis),
      exclude(src_p1, axis)
    };

    Vec2f src_interval{src_p0[axis], src_p1[axis]};
    const float src_interval_span = src_interval.y - src_interval.x;
    assert(src_interval_span > 0.0f);
    auto src_key = ray_project::make_non_adjacent_connection_key(src_ti, src0, src1);

    for (uint32_t j = 0; j < i1_size/2; j++) {
      uint32_t targ0 = i1[j * 2];
      uint32_t targ1 = i1[j * 2 + 1];
      auto targ_inds = tri::find_ti_with_edge(edge_indices, targ0, targ1);
      if (targ_inds.num_tis != 1) {
        //  Only try to connect if there is exactly one triangle with this edge. If there is more
        //  than one triangle with this edge, then it is already properly connected.
        continue;
      }

      Vec3f targ_p0;
      Vec3f targ_p1;
      memcpy(&targ_p0, verts + stride * targ0 + p_off, sizeof(Vec3f));
      memcpy(&targ_p1, verts + stride * targ1 + p_off, sizeof(Vec3f));

      if (targ_p1[axis] < targ_p0[axis]) {
        std::swap(targ0, targ1);
        std::swap(targ_p0, targ_p1);
      }

      const Vec2f check_targ[2] = {
        exclude(targ_p0, axis),
        exclude(targ_p1, axis),
      };

      bool candidate_targ_edge = true;
      for (auto& src_p : check_src) {
        for (auto& targ_p : check_targ) {
          auto diff = abs(src_p - targ_p);
          if (diff.x > tol || diff.y > tol) {
            candidate_targ_edge = false;
          }
        }
      }
      if (!candidate_targ_edge) {
        continue;
      }

      Vec2f targ_interval{targ_p0[axis], targ_p1[axis]};
      assert(targ_interval.y - targ_interval.x > 0.0f);

      bool isects = aabb_aabb_intersect_half_open(
        &src_interval.x, &src_interval.y, &targ_interval.x, &targ_interval.y, 1);
      if (!isects) {
        continue;
      }

      //  note, do use src_interval.x for both - this is the minimum coordinate.
      Vec2f rel_edge_coords{
        (targ_interval.x - src_interval.x) / src_interval_span,
        (targ_interval.y - src_interval.x) / src_interval_span,
      };
      assert(rel_edge_coords.x < rel_edge_coords.y);

      const uint32_t targ_ti = targ_inds.tis[0];
      const auto targ_key = ray_project::make_non_adjacent_connection_key(targ_ti, targ0, targ1);
      auto connect_entry = ray_project::make_non_adjacent_connection_entry(
        src_key, targ_key, rel_edge_coords);
      ray_project::push_pending_non_adjacent_connection(connections, connect_entry);
    }
  }
}

GROVE_NAMESPACE_END
