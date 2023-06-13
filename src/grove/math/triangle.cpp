#include "triangle.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

bool is_ccw_or_zero(const Vec3f& p0, const Vec3f& p1, const Vec3f& p2) {
  return det(p0, p1, p2) >= 0.0f;
}

} //  anon

void tri::compute_normals(const uint32_t* ti, uint32_t num_triangles,
                          const Vec3f* points, Vec3f* ns, uint32_t* cts, uint32_t ind_off) {
  for (uint32_t i = 0; i < num_triangles; i++) {
    auto ti0 = i * 3;
    for (uint32_t j = 0; j < 3; j++) {
      const auto i0 = j;
      const auto i1 = (j + 1) % 3;
      const auto i_prev = j == 0 ? 2 : i0 - 1;
      const auto pi0 = ti[ti0 + i0] - ind_off;
      const auto pi1 = ti[ti0 + i1] - ind_off;
      const auto pi_prev = ti[ti0 + i_prev] - ind_off;
      auto& p0 = points[pi0];
      auto& p1 = points[pi1];
      auto& p_prev = points[pi_prev];
      auto v0 = p1 - p0;
      auto v1 = p_prev - p0;
      auto n = normalize(cross(v0, v1));
      if (cts[pi0] == 0) {
        ns[pi0] = n;
        cts[pi0] = 1;
      } else {
        auto curr = ns[pi0] * float(cts[pi0]);
        curr = (curr + n) / float(cts[pi0] + 1);
        if (curr.length() > 0) {
          ns[pi0] = normalize(curr);
          cts[pi0]++;
        }
      }
    }
  }
}

void tri::compute_normals(const void* ti, uint32_t num_triangles, const void* points,
                          void* ns, void* cts, uint32_t index_offset, uint32_t point_stride,
                          uint32_t point_offset, uint32_t normal_stride, uint32_t normal_offset) {
  point_stride = point_stride == 0 ? sizeof(Vec3f) : point_stride;
  normal_stride = normal_stride == 0 ? sizeof(Vec3f) : normal_stride;

  const auto* cpoints = static_cast<const unsigned char*>(points);
  auto* cns = static_cast<unsigned char*>(ns);
  auto* ccts = static_cast<unsigned char*>(cts);
  auto* cti = static_cast<const unsigned char*>(ti);

  for (uint32_t i = 0; i < num_triangles; i++) {
    auto ti0 = i * 3;
    for (uint32_t j = 0; j < 3; j++) {
      const auto i0 = j;
      const auto i1 = (j + 1) % 3;
      const auto i_prev = j == 0 ? 2 : i0 - 1;

      uint32_t pi0;
      uint32_t pi1;
      uint32_t pi_prev;
      memcpy(&pi0, cti + (ti0 + i0) * sizeof(uint32_t), sizeof(uint32_t));
      memcpy(&pi1, cti + (ti0 + i1) * sizeof(uint32_t), sizeof(uint32_t));
      memcpy(&pi_prev, cti + (ti0 + i_prev) * sizeof(uint32_t), sizeof(uint32_t));
      assert(pi0 >= index_offset && pi1 >= index_offset && pi_prev >= index_offset);
      pi0 -= index_offset;
      pi1 -= index_offset;
      pi_prev -= index_offset;

      auto* pp0 = cpoints + pi0 * point_stride + point_offset;
      auto* pp1 = cpoints + pi1 * point_stride + point_offset;
      auto* pprev = cpoints + pi_prev * point_stride + point_offset;
      auto* np0 = cns + pi0 * normal_stride + normal_offset;
      auto* cp0 = ccts + pi0 * sizeof(uint32_t);

      Vec3f p0;
      Vec3f p1;
      Vec3f p_prev;
      memcpy(&p0, pp0, sizeof(Vec3f));
      memcpy(&p1, pp1, sizeof(Vec3f));
      memcpy(&p_prev, pprev, sizeof(Vec3f));

      auto v0 = p1 - p0;
      auto v1 = p_prev - p0;
      auto n = normalize(cross(v0, v1));

      uint32_t ct;
      memcpy(&ct, cp0, sizeof(uint32_t));
      if (ct == 0) {
        memcpy(np0, &n, sizeof(Vec3f));
        ct = 1;
        memcpy(cp0, &ct, sizeof(uint32_t));
      } else {
        Vec3f curr;
        memcpy(&curr, np0, sizeof(Vec3f));
        curr = (curr * float(ct) + n) / float(ct + 1);
        if (curr.length() > 0) {
          curr = normalize(curr);
          memcpy(np0, &curr, sizeof(Vec3f));
          ct++;
          memcpy(cp0, &ct, sizeof(uint32_t));
        }
      }
    }
  }
}

Vec3f tri::compute_normal(const Vec3f& p0, const Vec3f& p1, const Vec3f& p2) {
  auto v0 = p1 - p0;
  auto v1 = p2 - p0;
  return normalize(cross(normalize(v0), normalize(v1)));
}

void tri::compute_normals_per_triangle(const uint32_t* ti, uint32_t num_triangles,
                                       const Vec3f* points, Vec3f* ns) {
  for (uint32_t i = 0; i < num_triangles; i++) {
    auto i0 = ti[i * 3];
    auto i1 = ti[i * 3 + 1];
    auto i_prev = ti[i * 3 + 2];
    auto v0 = points[i1] - points[i0];
    auto v1 = points[i_prev] - points[i0];
    auto n = cross(normalize(v0), normalize(v1));
    ns[i] = normalize(n);
  }
}

bool tri::is_ccw(const Vec3f& p0, const Vec3f& p1, const Vec3f& p2) {
  return det(p0, p1, p2) > 0.0f;
}

bool tri::is_ccw(const uint32_t* tris, uint32_t num_tris, const Vec3f* ps) {
  for (uint32_t i = 0; i < num_tris; i++) {
    const uint32_t* tri = tris + i * 3;
    if (!is_ccw(ps[tri[0]], ps[tri[1]], ps[tri[2]])) {
      return false;
    }
  }
  return true;
}

bool tri::is_ccw_or_zero(const uint32_t* tris, uint32_t num_tris, const Vec3f* ps) {
  for (uint32_t i = 0; i < num_tris; i++) {
    const uint32_t* tri = tris + i * 3;
    if (!grove::is_ccw_or_zero(ps[tri[0]], ps[tri[1]], ps[tri[2]])) {
      return false;
    }
  }
  return true;
}

void tri::require_ccw(uint32_t* tris, uint32_t num_tris, const Vec3f* ps) {
  for (uint32_t i = 0; i < num_tris; i++) {
    auto& pi0 = tris[i * 3];
    auto& pi1 = tris[i * 3 + 1];
    auto& pi2 = tris[i * 3 + 2];
    auto& p0 = ps[pi0];
    auto& p1 = ps[pi1];
    auto& p2 = ps[pi2];
    if (!tri::is_ccw(p0, p1, p2)) {
      std::swap(pi2, pi1);
    }
  }
}

void tri::require_ccw(void* tris, uint32_t num_tris,
                      void* ps, uint32_t p_stride, uint32_t p_offset,
                      uint32_t index_offset) {
  auto* tris_char = static_cast<unsigned char*>(tris);
  auto* ps_char = static_cast<const unsigned char*>(ps);

  for (uint32_t i = 0; i < num_tris; i++) {
    const uint32_t ti = i * 3;

    uint32_t pi0;
    uint32_t pi1;
    uint32_t pi2;
    memcpy(&pi0, tris_char + (ti + 0) * sizeof(uint32_t), sizeof(uint32_t));
    memcpy(&pi1, tris_char + (ti + 1) * sizeof(uint32_t), sizeof(uint32_t));
    memcpy(&pi2, tris_char + (ti + 2) * sizeof(uint32_t), sizeof(uint32_t));
    assert(pi0 >= index_offset && pi1 >= index_offset && pi2 >= index_offset);

    Vec3f p0;
    Vec3f p1;
    Vec3f p2;
    memcpy(&p0, ps_char + (pi0 - index_offset) * p_stride + p_offset, sizeof(Vec3f));
    memcpy(&p1, ps_char + (pi1 - index_offset) * p_stride + p_offset, sizeof(Vec3f));
    memcpy(&p2, ps_char + (pi2 - index_offset) * p_stride + p_offset, sizeof(Vec3f));

    if (!is_ccw(p0, p1, p2)) {
      std::swap(pi2, pi1);
      assert(grove::is_ccw_or_zero(p0, p2, p1));
      memcpy(tris_char + (ti + 1) * sizeof(uint32_t), &pi1, sizeof(uint32_t));
      memcpy(tris_char + (ti + 2) * sizeof(uint32_t), &pi2, sizeof(uint32_t));
    }
  }
}

uint32_t tri::find_adjacent_order_independent(const uint32_t* tris, uint32_t num_triangles,
                                              uint32_t src, uint32_t ai, uint32_t bi) {
  assert(ai != bi);
  for (uint32_t i = 0; i < num_triangles; i++) {
    if (i != src) {
      int ct{};
      for (int j = 0; j < 3; j++) {
        auto pi = tris[i * 3 + j];
        ct += int(pi == ai);
        ct += int(pi == bi);
      }
      if (ct == 2) {
        return i;
      }
    }
  }
  return no_adjacent_triangle();
}

uint32_t tri::setdiff_edge(const uint32_t* tri, uint32_t ai, uint32_t bi) {
  for (int i = 0; i < 3; i++) {
    if (tri[i] != ai && tri[i] != bi) {
      return tri[i];
    }
  }
  assert(false);
  return no_adjacent_triangle();
}

void tri::setdiff_point(const uint32_t* tri, uint32_t pi, uint32_t* ai, uint32_t* bi) {
  for (int i = 0; i < 3; i++) {
    if (tri[i] == pi) {
      int i1 = (i + 1) % 3;
      int i2 = (i1 + 1) % 3;
      *ai = tri[i1];
      *bi = tri[i2];
      assert(*ai != pi && *bi != pi);
      return;
    }
  }
  assert(false);
}

bool tri::contains_point(const uint32_t* tri, uint32_t pi) {
  for (int i = 0; i < 3; i++) {
    if (tri[i] == pi) {
      return true;
    }
  }
  return false;
}

GROVE_NAMESPACE_END
