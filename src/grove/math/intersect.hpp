#pragma once

#include "vector.hpp"
#include "matrix.hpp"
#include "Bounds2.hpp"
#include "Bounds3.hpp"
#include "OBB3.hpp"
#include "Ray.hpp"
#include <cmath>

namespace grove {

struct OBBIntersectToProjectedAABBResult {
  bool accept;
  bool found_aabb;
  Bounds3f aabb;
};

struct Frustum;

OBBIntersectToProjectedAABBResult
obb_intersect_to_projected_aabb(const OBB3f& target, const OBB3f& query,
                                int forward_dim, bool constrain_to_query);

inline bool quadratic(float a, float b, float c, float* t0, float* t1) {
  auto db = double(b);
  auto da = double(a);
  auto dc = double(c);

  double discriminant = db * db - 4.0 * da * dc;
  if (discriminant < 0.0) {
    return false;
  }

  double root_discriminant = std::sqrt(discriminant);
  double q = db < 0.0 ? -0.5 * (db - root_discriminant) : -0.5 * (db + root_discriminant);

  *t0 = float(q / da);
  *t1 = float(dc / q);

  if (*t0 > *t1) {
    std::swap(*t0, *t1);
  }

  return true;
}

bool ray_plane_intersect(const Ray& ray, const Vec4f& plane, float* t);
bool ray_plane_intersect(const Vec3f& ro, const Vec3f& rd, const Vec4f& plane, float* t);

bool ray_sphere_intersect(const Ray& ray, const Vec3f& p, float sphere_radius,
                          float* t0, float* t1);

bool frustum_aabb_intersect(const Frustum& f, const Bounds3f& aabb);

template <typename T>
inline bool aabb_sphere_intersect(const Bounds3<T>& aabb, const Vec3<T>& c, T r) {
  T dist{};

  for (int i = 0; i < 3; i++) {
    auto v = c[i];
    if (v < aabb.min[i]) {
      auto d = aabb.min[i] - v;
      dist += d * d;
    }

    if (v > aabb.max[i]) {
      auto d = v - aabb.max[i];
      dist += d * d;
    }
  }

  return dist <= r * r;
}

template <typename T>
bool aabb_aabb_intersect_closed(const T* a0, const T* a1, const T* b0, const T* b1, int nd) {
  for (int i = 0; i < nd; i++) {
    if (a0[i] <= b0[i]) {
      if (a1[i] < b0[i]) {
        return false;
      }
    } else {
      if (b1[i] < a0[i]) {
        return false;
      }
    }
  }
  return true;
}

template <typename T>
bool aabb_aabb_intersect_half_open(const T* a0, const T* a1, const T* b0, const T* b1, int nd) {
  for (int i = 0; i < nd; i++) {
    if (a0[i] <= b0[i]) {
      if (!(a1[i] > b0[i])) {
        return false;
      }
    } else {
      if (!(b1[i] > a0[i])) {
        return false;
      }
    }
  }
  return true;
}

template <typename T>
bool aabb_aabb_intersect_closed(const Bounds2<T>& a, const Bounds2<T>& b) {
  return aabb_aabb_intersect_closed(&a.min.x, &a.max.x, &b.min.x, &b.max.x, 2);
}

template <typename T>
bool aabb_aabb_intersect_closed(const Bounds3<T>& a, const Bounds3<T>& b) {
  return aabb_aabb_intersect_closed(&a.min.x, &a.max.x, &b.min.x, &b.max.x, 3);
}

template <typename T>
bool aabb_aabb_intersect_half_open(const Bounds2<T>& a, const Bounds2<T>& b) {
  return aabb_aabb_intersect_half_open(&a.min.x, &a.max.x, &b.min.x, &b.max.x, 2);
}

template <typename T>
bool aabb_aabb_intersect_half_open(const Bounds3<T>& a, const Bounds3<T>& b) {
  return aabb_aabb_intersect_half_open(&a.min.x, &a.max.x, &b.min.x, &b.max.x, 3);
}

inline bool ray_circle_intersect(const Vec2f& o, const Vec2f& d, const Vec2f& p,
                                 float sphere_radius, float* t0, float* t1) {
  const float a = dot(d, d);
  const float b = 2.0f * dot(o, d) - 2.0f * dot(p, d);
  const float c = dot(o, o) - 2.0f * dot(p, o) + dot(p, p) - sphere_radius;

  return quadratic(a, b, c, t0, t1);
}

template <typename T>
inline bool point_circle_intersect(const T& p, const T& cp, float radius) {
  auto to_circle = cp - p;
  auto thresh = radius * radius;
  return dot(to_circle, to_circle) <= thresh;
}

bool ray_aabb_intersect(const Ray& ray, const Bounds3f& aabb, float* t0, float* t1);
bool ray_aabb_intersect(const Vec3f& ro, const Vec3f& rd, const Vec3f& p0, const Vec3f& p1,
                        float* t0, float* t1);

bool point_aabb_intersect(const Vec2f& coords, const Vec2f& p0, const Vec2f& p1);

template <typename T>
bool ray_triangle_intersect(const Vec3<T>& ro, const Vec3<T>& rd,
                            const Vec3<T>& p0, const Vec3<T>& p1,
                            const Vec3<T>& p2, T* t) {
  //  Physically Based Rendering, From Theory to Implementation. pp 157.

  //  Translate vertices to ray origin.
  auto p0t = p0 - ro;
  auto p1t = p1 - ro;
  auto p2t = p2 - ro;

  //  Permute dimensions so that largest component of `rd` forms the z-axis.
  int z_ind = max_dimension(abs(rd));
  int x_ind = (z_ind + 1) % 3;
  int y_ind = (x_ind + 1) % 3;

  auto rdt = permute(rd, x_ind, y_ind, z_ind);
  p0t = permute(p0t, x_ind, y_ind, z_ind);
  p1t = permute(p1t, x_ind, y_ind, z_ind);
  p2t = permute(p2t, x_ind, y_ind, z_ind);

  //  Shear components according to transformation that aligns `rd`
  //  with the positive z-axis.
  const Vec3<T> shear{-rdt.x/rdt.z, -rdt.y/rdt.z, T(1)/rdt.z};

  p0t.x += shear.x * p0t.z;
  p0t.y += shear.y * p0t.z;
  p0t.z *= shear.z;

  p1t.x += shear.x * p1t.z;
  p1t.y += shear.y * p1t.z;
  p1t.z *= shear.z;

  p2t.x += shear.x * p2t.z;
  p2t.y += shear.y * p2t.z;
  p2t.z *= shear.z;

  auto e0 = p1t.x * p2t.y - p1t.y * p2t.x;
  auto e1 = p2t.x * p0t.y - p2t.y * p0t.x;
  auto e2 = p0t.x * p1t.y - p0t.y * p1t.x;

  if (sizeof(T) == sizeof(float) &&
      (e0 == T(0) || e1 == T(0) || e2 == T(0))) {
    // Evaluate in double.
    auto p2txp1ty = double(p2t.x) * double(p1t.y);
    auto p2typ1tx = double(p2t.y) * double(p1t.x);
    e0 = T(p2typ1tx - p2txp1ty);

    auto p0txp2ty = double(p0t.x) * double(p2t.y);
    auto p0typ2tx = double(p0t.y) * double(p2t.x);
    e1 = T(p0typ2tx - p0txp2ty);

    auto p1txp0ty = double(p1t.x) * double(p0t.y);
    auto p1typ0tx = double(p1t.y) * double(p0t.x);
    e2 = T(p1typ0tx - p1txp0ty);
  }

  if ((e0 < 0 || e1 < 0 || e2 < 0) &&
      (e0 > 0 || e1 > 0 || e2 > 0)) {
    //  sign mismatch
    return false;
  }

  auto det = e0 + e1 + e2;
  if (det == 0) {
    return false;
  }

  const auto t_scaled = e0 * p0t.z + e1 * p1t.z + e2 * p2t.z;

  if ((det < 0 && t_scaled >= 0) || (det > 0 && t_scaled <= 0)) {
    return false;
  }

  *t = t_scaled / det;

  return true;
}

template <typename Indices>
bool ray_triangle_intersect(const Ray& ray, const Vec3f* p,
                            Indices* tri_inds, int num_tris,
                            int* hit_tri, float* t) {
  auto t_val = std::numeric_limits<float>::max();
  int hit_tri_index = -1;
  bool any_intersect = false;

  for (int i = 0; i < num_tris; i++) {
    auto p0 = p[tri_inds[i * 3]];
    auto p1 = p[tri_inds[i * 3 + 1]];
    auto p2 = p[tri_inds[i * 3 + 2]];
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

template <typename Indices>
bool ray_triangle_intersect(const Ray& ray, const float* p,
                            Indices* tri_inds, int num_tris,
                            int* hit_tri, float* t) {
  auto t_val = std::numeric_limits<float>::max();
  int hit_tri_index = -1;
  bool any_intersect = false;

  const auto to_vec3 = [&](int i, int off) {
    auto ind = tri_inds[i * 3 + off] * 3;
    return Vec3f{p[ind], p[ind + 1], p[ind + 2]};
  };

  for (int i = 0; i < num_tris; i++) {
    auto p0 = to_vec3(i, 0);
    auto p1 = to_vec3(i, 1);
    auto p2 = to_vec3(i, 2);
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

template <typename Indices>
bool ray_triangle_intersect(const Ray& ray, const float* p, const Mat4f& transform,
                            Indices* tri_inds, int num_tris,
                            int* hit_tri, float* t) {
  auto t_val = std::numeric_limits<float>::max();
  int hit_tri_index = -1;
  bool any_intersect = false;

  const auto to_vec4 = [&](int i, int off) {
    auto ind = tri_inds[i * 3 + off] * 3;
    return Vec4f{p[ind], p[ind + 1], p[ind + 2], 1.0f};
  };

  for (int i = 0; i < num_tris; i++) {
    auto p0 = to_vec3(transform * to_vec4(i, 0));
    auto p1 = to_vec3(transform * to_vec4(i, 1));
    auto p2 = to_vec3(transform * to_vec4(i, 2));
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

bool ray_triangle_intersect(const Ray& ray, const float* data,
                            int num_tris, int stride, int off,
                            int* hit_tri, float* t);

bool ray_triangle_intersect(const Ray& ray, const float* data, const Mat4f& transform,
                            int num_tris, int stride, int off,
                            int* hit_tri, float* t);

bool ray_triangle_intersect(const Ray& ray, const unsigned char* data,
                            size_t num_tris, uint32_t stride, uint32_t off,
                            size_t* hit_tri, float* t);

bool ray_triangle_intersect(const Ray& ray, const unsigned char* data, const Mat4f& transform,
                            size_t num_tris, uint32_t stride, uint32_t off,
                            size_t* hit_tri, float* t);

template <typename Indices>
bool ray_triangle_intersect(const Ray& ray, const Vec3f* p, const Mat4f& transform,
                            Indices* tri_inds, int num_tris, int* hit_tri, float* t) {
  auto t_val = std::numeric_limits<float>::max();
  int hit_tri_index = -1;
  bool any_intersect = false;

  for (int i = 0; i < num_tris; i++) {
    auto p0 = p[tri_inds[i * 3]];
    auto p1 = p[tri_inds[i * 3 + 1]];
    auto p2 = p[tri_inds[i * 3 + 2]];

    p0 = to_vec3(transform * Vec4f(p0, 1.0f));
    p1 = to_vec3(transform * Vec4f(p1, 1.0f));
    p2 = to_vec3(transform * Vec4f(p2, 1.0f));

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

template <typename Indices>
bool ray_triangle_intersect(const Ray& ray, const void* data, size_t stride, size_t off,
                            const Indices* indices, size_t num_tris, int32_t index_offset,
                            const Mat4f* optional_transform,
                            size_t* hit_tri, float* t) {
  const auto* data_char = static_cast<const unsigned char*>(data);

  float t_val = std::numeric_limits<float>::max();
  size_t hit_tri_index = ~size_t(0);
  bool any_intersect = false;

  uint32_t index_offset_u32{};
  bool sub_offset{};
  if (index_offset < 0) {
    sub_offset = true;
    index_offset_u32 = uint32_t(std::abs(index_offset));
  } else {
    index_offset_u32 = uint32_t(index_offset);
  }

  for (size_t i = 0; i < num_tris; i++) {
    auto pi0 = uint32_t(indices[i * 3 + 0]);
    auto pi1 = uint32_t(indices[i * 3 + 1]);
    auto pi2 = uint32_t(indices[i * 3 + 2]);
    if (sub_offset) {
      assert(pi0 >= index_offset_u32 && pi1 >= index_offset_u32 && pi2 >= index_offset_u32);
      pi0 -= index_offset_u32;
      pi1 -= index_offset_u32;
      pi2 -= index_offset_u32;
    } else {
      pi0 += index_offset_u32;
      pi1 += index_offset_u32;
      pi2 += index_offset_u32;
    }
    Vec3f p0;
    memcpy(&p0, data_char + stride * pi0 + off, sizeof(Vec3f));
    Vec3f p1;
    memcpy(&p1, data_char + stride * pi1 + off, sizeof(Vec3f));
    Vec3f p2;
    memcpy(&p2, data_char + stride * pi2 + off, sizeof(Vec3f));
    if (optional_transform) {
      p0 = to_vec3(*optional_transform * Vec4f{p0, 1.0f});
      p1 = to_vec3(*optional_transform * Vec4f{p1, 1.0f});
      p2 = to_vec3(*optional_transform * Vec4f{p2, 1.0f});
    }

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

namespace detail {

template <typename T>
T projected_radius(const OBB3<T>& a, const Vec3<T>& l) {
  return std::abs(dot(a.i * a.half_size.x, l)) +
         std::abs(dot(a.j * a.half_size.y, l)) +
         std::abs(dot(a.k * a.half_size.z, l));
}

template <typename T>
bool overlapping_axis(const OBB3<T>& a, const OBB3<T>& b, const Vec3<T>& t, const Vec3<T>& l) {
  auto thresh = std::abs(dot(t, l));
  auto ra = projected_radius(a, l);
  auto rb = projected_radius(b, l);
  return thresh <= ra + rb;
}

} //  detail

template <typename T>
inline bool obb_obb_intersect(const OBB3<T>& a, const OBB3<T>& b) {
  const auto t = b.position - a.position;
  return detail::overlapping_axis(a, b, t, a.i) &&
         detail::overlapping_axis(a, b, t, a.j) &&
         detail::overlapping_axis(a, b, t, a.k) &&
         detail::overlapping_axis(a, b, t, b.i) &&
         detail::overlapping_axis(a, b, t, b.j) &&
         detail::overlapping_axis(a, b, t, b.k) &&
         detail::overlapping_axis(a, b, t, cross(a.i, b.i)) &&
         detail::overlapping_axis(a, b, t, cross(a.i, b.j)) &&
         detail::overlapping_axis(a, b, t, cross(a.i, b.k)) &&
         detail::overlapping_axis(a, b, t, cross(a.j, b.i)) &&
         detail::overlapping_axis(a, b, t, cross(a.j, b.j)) &&
         detail::overlapping_axis(a, b, t, cross(a.j, b.k)) &&
         detail::overlapping_axis(a, b, t, cross(a.k, b.i)) &&
         detail::overlapping_axis(a, b, t, cross(a.k, b.j)) &&
         detail::overlapping_axis(a, b, t, cross(a.k, b.k));
}

inline bool ray_obb_intersect(const Vec3f& ro, const Vec3f& rd,
                              const OBB3f& obb, float* t0, float* t1) {
  Mat3f m{obb.i, obb.j, obb.k};
  m = transpose(m);
  Ray trans_ray{m * (ro - obb.position), m * rd};
  Bounds3f aabb{-obb.half_size, obb.half_size};
  return ray_aabb_intersect(trans_ray, aabb, t0, t1);
}

bool ray_capped_cylinder_intersect(const Vec3f& src_ro, const Vec3f& src_rd,
                                   const Mat3f& cyl_frame, const Vec3f& cyl_p,
                                   float cyl_r, float cyl_half_l, float* out_t);

Vec3f mouse_ray_direction(const Mat4f& inv_view,
                          const Mat4f& inv_proj,
                          const Vec2f& mouse_pixel_coords,
                          const Vec2f& window_pixel_dimensions);

}