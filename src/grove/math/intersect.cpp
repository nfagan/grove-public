#include "intersect.hpp"
#include "grove/common/common.hpp"
#include "Ray.hpp"
#include "Frustum.hpp"
#include "constants.hpp"
#include <algorithm>

GROVE_NAMESPACE_BEGIN

namespace {

inline Vec3f read_vec3_from_vec3_byte_sequence(const unsigned char* data, std::size_t i) {
  constexpr auto fs = sizeof(float);
  float x;
  std::memcpy(&x, data + i, fs);
  float y;
  std::memcpy(&y, data + i + fs, fs);
  float z;
  std::memcpy(&z, data + i + fs * 2, fs);
  return Vec3f{x, y, z};
}

OBBIntersectToProjectedAABBResult check_obb_vertices(const Vec3f* verts, const Vec3f& rd,
                                                     const Vec3f& targ_half_size,
                                                     int forward_dim,
                                                     bool constrain_to_query) {
  Vec4f plane_target_face0{};
  Vec4f plane_target_face1{};

  plane_target_face0[forward_dim] = -1;
  plane_target_face0[3] = -targ_half_size[forward_dim];

  plane_target_face1[forward_dim] = 1;
  plane_target_face1[3] = -targ_half_size[forward_dim];

  Vec3f hp[32];
  int num_hp{};
  for (int i = 0; i < 8; i++) {
    const auto& vi = verts[i];
    float t;
    if (ray_plane_intersect(vi, rd, plane_target_face0, &t)) {
      hp[num_hp++] = vi + rd * t;
    }
    if (ray_plane_intersect(vi, rd, plane_target_face1, &t)) {
      hp[num_hp++] = vi + rd * t;
    }
    if (ray_plane_intersect(vi, -rd, plane_target_face0, &t)) {
      hp[num_hp++] = vi - rd * t;
    }
    if (ray_plane_intersect(vi, -rd, plane_target_face1, &t)) {
      hp[num_hp++] = vi - rd * t;
    }
  }

  OBBIntersectToProjectedAABBResult result{};
  if (num_hp == 0) {
    return result;
  }

  Vec3f mn;
  Vec3f mx;
  union_of(hp, num_hp, &mn, &mx);
  mn[forward_dim] = -targ_half_size[forward_dim];
  mx[forward_dim] = targ_half_size[forward_dim];

  if (constrain_to_query) {
    Vec3f qmn;
    Vec3f qmx;
    union_of(verts, 8, &qmn, &qmx);
    mn = max(mn, qmn);
    mx = min(mx, qmx);
  }
  auto tot_mx = max(abs(mn), abs(mx));

  result.aabb = Bounds3f{mn, mx};
  result.found_aabb = true;
  result.accept = true;
  for (int i = 0; i < 3; i++) {
    if (i != forward_dim && tot_mx[i] >= targ_half_size[i]) {
      result.accept = false;
      break;
    }
  }

  return result;
}

int aabb_plane_side(const Vec4f& p, const Vec3f* vs) {
  auto n = to_vec3(p);
  bool any_pos{};
  bool any_neg{};
  for (int i = 0; i < 8; i++) {
    const float sdf = dot(n, vs[i]) + p.w;
    any_pos |= (sdf >= 0.0f);
    any_neg |= (sdf < 0.0f);
  }
  assert(any_pos || any_neg);
  if (any_pos && any_neg) {
    return 0;
  } else if (!any_neg) {
    return 1;
  } else {
    assert(!any_pos);
    return -1;
  }
}

} //  anon

OBBIntersectToProjectedAABBResult
obb_intersect_to_projected_aabb(const OBB3f& target, const OBB3f& query,
                                int forward_dim, bool constrain_to_query) {
  assert(forward_dim >= 0 && forward_dim < 3);
  Vec3f ti = target.i;
  Vec3f tj = target.j;
  Vec3f tk = target.k;
  invert_implicit_3x3(ti, tj, tk, &ti, &tj, &tk);

  auto to_target = query.position - target.position;
  auto trans_to_target = ti * to_target.x + tj * to_target.y + tk * to_target.z;

  auto inv_q = query;
  mul_implicit_3x3(ti, tj, tk, query.i, query.j, query.k, &inv_q.i, &inv_q.j, &inv_q.k);
  inv_q.position = trans_to_target;

  Vec3f vs[8];
  gather_vertices(inv_q, vs);
  auto f = forward_dim == 0 ? inv_q.i : forward_dim == 1 ? inv_q.j : inv_q.k;
  f = normalize(f);
  return check_obb_vertices(vs, f, target.half_size, forward_dim, constrain_to_query);
}

bool ray_plane_intersect(const Ray& ray, const Vec4f& plane, float* t) {
  const auto n = Vec3f(plane.x, plane.y, plane.z);
  const auto denom = dot(n, ray.direction);

  if (denom == 0) {
    *t = 0.0f;
    return false;
  }

  const auto num = -dot(n, ray.origin) - plane.w;
  *t = num / denom;

  return true;
}

bool ray_plane_intersect(const Vec3f& ro, const Vec3f& rd, const Vec4f& plane, float* t) {
  const auto n = Vec3f(plane.x, plane.y, plane.z);
  const auto denom = dot(n, rd);
  if (denom == 0) {
    *t = 0.0f;
    return false;
  }
  const auto num = -dot(n, ro) - plane.w;
  *t = num / denom;
  return true;
}

bool ray_sphere_intersect(const Ray& ray, const Vec3f& p, float sphere_radius,
                          float* t0, float* t1) {
  const auto& o = ray.origin;
  const auto& d = ray.direction;

  const float a = dot(d, d);
  const float b = 2.0f * dot(o, d) - 2.0f * dot(p, d);
  const float c = dot(o, o) - 2.0f * dot(p, o) + dot(p, p) - sphere_radius;

  return quadratic(a, b, c, t0, t1);
}

bool ray_aabb_intersect(const Ray& ray, const Bounds3f& aabb, float* t0, float* t1) {
  float tmp_t0 = -grove::infinityf();
  float tmp_t1 = grove::infinityf();

  for (int i = 0; i < 3; i++) {
    float inv_d = 1.0f / ray.direction[i];
    float t00 = (aabb.min[i] - ray.origin[i]) * inv_d;
    float t11 = (aabb.max[i] - ray.origin[i]) * inv_d;

    if (t00 > t11) {
      std::swap(t00, t11);
    }

    tmp_t0 = std::max(tmp_t0, t00);
    tmp_t1 = std::min(tmp_t1, t11);

    if (tmp_t0 > tmp_t1) {
      return false;
    }
  }

  *t0 = tmp_t0;
  *t1 = tmp_t1;

  return true;
}

bool ray_aabb_intersect(const Vec3f& ro, const Vec3f& rd, const Vec3f& p0, const Vec3f& p1,
                        float* t0, float* t1) {
  float tmp_t0 = -grove::infinityf();
  float tmp_t1 = grove::infinityf();

  for (int i = 0; i < 3; i++) {
    float inv_d = 1.0f / rd[i];
    float t00 = (p0[i] - ro[i]) * inv_d;
    float t11 = (p1[i] - ro[i]) * inv_d;

    if (t00 > t11) {
      std::swap(t00, t11);
    }

    tmp_t0 = std::max(tmp_t0, t00);
    tmp_t1 = std::min(tmp_t1, t11);

    if (tmp_t0 > tmp_t1) {
      return false;
    }
  }

  *t0 = tmp_t0;
  *t1 = tmp_t1;

  return true;
}

bool frustum_aabb_intersect(const Frustum& f, const Bounds3f& aabb) {
  Vec3f vs[8];
  gather_vertices(aabb, vs);
  return aabb_plane_side(f.array[0], vs) >= 0 &&
         aabb_plane_side(f.array[1], vs) >= 0 &&
         aabb_plane_side(f.array[2], vs) >= 0 &&
         aabb_plane_side(f.array[3], vs) >= 0 &&
         aabb_plane_side(f.array[4], vs) >= 0 &&
         aabb_plane_side(f.array[5], vs) >= 0;
}

bool point_aabb_intersect(const Vec2f& coords, const Vec2f& p0, const Vec2f& p1) {
  return coords.x >= p0.x && coords.y >= p0.y &&
         coords.x < p1.x && coords.y < p1.y;
}

bool ray_triangle_intersect(const Ray& ray, const float* data,
                            int num_tris, int stride, int off,
                            int* hit_tri, float* t) {
  auto t_val = std::numeric_limits<float>::max();
  int hit_tri_index = -1;
  bool any_intersect = false;

  const auto to_vec3 = [&](int i) {
    return Vec3f{data[i], data[i + 1], data[i + 2]};
  };

  for (int i = 0; i < num_tris; i++) {
    auto p0_ind = i * 3;
    auto p1_ind = p0_ind + 1;
    auto p2_ind = p0_ind + 2;

    auto i0 = p0_ind * stride + off;
    auto i1 = p1_ind * stride + off;
    auto i2 = p2_ind * stride + off;

    auto p0 = to_vec3(i0);
    auto p1 = to_vec3(i1);
    auto p2 = to_vec3(i2);

    float tmp_t;
    bool hit = ray_triangle_intersect(ray.origin, ray.direction, p0, p1, p2, &tmp_t);

    if (hit && tmp_t < t_val) {
      t_val = tmp_t;
      hit_tri_index = i;
      any_intersect = true;
    }
  }

  if (any_intersect) {
    *hit_tri = hit_tri_index;
    *t = t_val;
  }

  return any_intersect;
}

bool ray_triangle_intersect(const Ray& ray, const float* data, const Mat4f& transform,
                            int num_tris, int stride, int off,
                            int* hit_tri, float* t) {
  auto t_val = std::numeric_limits<float>::max();
  int hit_tri_index = -1;
  bool any_intersect = false;

  const auto make_transformed_vec3 = [&](int i) {
    return to_vec3(transform * Vec4f{data[i], data[i + 1], data[i + 2], 1.0f});
  };

  for (int i = 0; i < num_tris; i++) {
    auto p0_ind = i * 3;
    auto p1_ind = p0_ind + 1;
    auto p2_ind = p0_ind + 2;

    auto i0 = p0_ind * stride + off;
    auto i1 = p1_ind * stride + off;
    auto i2 = p2_ind * stride + off;

    auto p0 = make_transformed_vec3(i0);
    auto p1 = make_transformed_vec3(i1);
    auto p2 = make_transformed_vec3(i2);

    float tmp_t;
    bool hit = ray_triangle_intersect(ray.origin, ray.direction, p0, p1, p2, &tmp_t);

    if (hit && tmp_t < t_val) {
      t_val = tmp_t;
      hit_tri_index = i;
      any_intersect = true;
    }
  }

  if (any_intersect) {
    *hit_tri = hit_tri_index;
    *t = t_val;
  }

  return any_intersect;
}

bool ray_triangle_intersect(const Ray& ray, const unsigned char* data,
                            size_t num_tris, uint32_t stride, uint32_t off,
                            size_t* hit_tri, float* t) {
  auto t_val = std::numeric_limits<float>::max();
  size_t hit_tri_index{};
  bool any_intersect = false;

  for (size_t i = 0; i < num_tris; i++) {
    auto p0_ind = i * 3;
    auto p1_ind = p0_ind + 1;
    auto p2_ind = p0_ind + 2;

    auto i0 = p0_ind * stride + off;
    auto i1 = p1_ind * stride + off;
    auto i2 = p2_ind * stride + off;

    auto p0 = read_vec3_from_vec3_byte_sequence(data, i0);
    auto p1 = read_vec3_from_vec3_byte_sequence(data, i1);
    auto p2 = read_vec3_from_vec3_byte_sequence(data, i2);

    float tmp_t;
    bool hit = ray_triangle_intersect(ray.origin, ray.direction, p0, p1, p2, &tmp_t);

    if (hit && tmp_t < t_val) {
      t_val = tmp_t;
      hit_tri_index = i;
      any_intersect = true;
    }
  }

  if (any_intersect) {
    *t = t_val;
    *hit_tri = hit_tri_index;
  }

  return any_intersect;
}

bool ray_triangle_intersect(const Ray& ray, const unsigned char* data,
                            const Mat4f& transform,
                            size_t num_tris, uint32_t stride, uint32_t off,
                            size_t* hit_tri, float* t) {
  auto t_val = std::numeric_limits<float>::max();
  size_t hit_tri_index{};
  bool any_intersect = false;

  for (size_t i = 0; i < num_tris; i++) {
    auto p0_ind = i * 3;
    auto p1_ind = p0_ind + 1;
    auto p2_ind = p0_ind + 2;

    auto i0 = p0_ind * stride + off;
    auto i1 = p1_ind * stride + off;
    auto i2 = p2_ind * stride + off;

    auto p0 = to_vec3(transform * Vec4f{read_vec3_from_vec3_byte_sequence(data, i0), 1.0f});
    auto p1 = to_vec3(transform * Vec4f{read_vec3_from_vec3_byte_sequence(data, i1), 1.0f});
    auto p2 = to_vec3(transform * Vec4f{read_vec3_from_vec3_byte_sequence(data, i2), 1.0f});

    float tmp_t;
    bool hit = ray_triangle_intersect(ray.origin, ray.direction, p0, p1, p2, &tmp_t);

    if (hit && tmp_t < t_val) {
      t_val = tmp_t;
      hit_tri_index = i;
      any_intersect = true;
    }
  }

  if (any_intersect) {
    *t = t_val;
    *hit_tri = hit_tri_index;
  }

  return any_intersect;
}

bool ray_capped_cylinder_intersect(const Vec3f& src_ro, const Vec3f& src_rd,
                                   const Mat3f& frame, const Vec3f& p, float r, float half_l,
                                   float* out_t) {
  //  https://pbr-book.org/3ed-2018/Shapes/Spheres#fragment-Solvequadraticequationformonotvalues-0
  float min_t{infinityf()};
  bool any_hit{};
  const Vec3f up = frame[1];
  {
    auto plane_top_p = p + up * half_l;
    auto plane_top = Vec4f{up, -dot(up, plane_top_p)};

    float t;
    if (ray_plane_intersect(src_ro, src_rd, plane_top, &t) && t > 0.0f && t < min_t) {
      if (((src_ro + src_rd * t) - plane_top_p).length() < r) {
        min_t = t;
        any_hit = true;
      }
    }
  }
  {
    auto plane_bot_p = p - up * half_l;
    auto plane_bot = Vec4f{-up, -dot(-up, plane_bot_p)};

    float t;
    if (ray_plane_intersect(src_ro, src_rd, plane_bot, &t) && t > 0.0f && t < min_t) {
      if (((src_ro + src_rd * t) - plane_bot_p).length() < r) {
        min_t = t;
        any_hit = true;
      }
    }
  }
  {
    auto inv_frame = transpose(frame);
    auto ro_to_node = src_ro - p;
    auto ro = inv_frame * ro_to_node;
    auto rd = inv_frame * src_rd;
    float a = rd.x * rd.x + rd.z * rd.z;
    float b = 2.0f * (rd.x * ro.x + rd.z * ro.z);
    float c = ro.x * ro.x + ro.z * ro.z - r * r;

    float t0;
    float t1;
    if (quadratic(a, b, c, &t0, &t1) && t0 > 0.0f && t0 < min_t) {
      auto hit_p = ro + rd * t0;
      if (hit_p.y >= -half_l && hit_p.y < half_l) {
        min_t = t0;
        any_hit = true;
      }
    }
  }

  if (any_hit) {
    *out_t = min_t;
  }

  return any_hit;
}

Vec3f mouse_ray_direction(const Mat4f& inv_view,
                          const Mat4f& inv_proj,
                          const Vec2f& mouse_pixel_coords,
                          const Vec2f& window_pixel_dimensions) {
  //
  auto frac_x = mouse_pixel_coords.x / window_pixel_dimensions.x;
  auto frac_y = mouse_pixel_coords.y / window_pixel_dimensions.y;

  const float x = frac_x * 2.0f - 1.0f;
  const float y = (1.0f - frac_y) * 2.0f - 1.0f;
  Vec4f pos(x, y, 1.0f, 1.0f);

  pos = inv_proj * pos;
  pos.z = 1.0f;
  pos.w = 0.0f;
  pos = normalize(inv_view * pos);

  return {pos.x, pos.y, pos.z};
}

GROVE_NAMESPACE_END