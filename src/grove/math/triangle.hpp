#pragma once

#include "Vec3.hpp"

namespace grove::tri {

void compute_normals(const uint32_t* ti,      //  size = num_triangles * 3
                     uint32_t num_triangles,
                     const Vec3f* points,
                     Vec3f* ns,               //  size = number of points
                     uint32_t* zeroed_cts,
                     //  subtract `index_offset` before referencing `points`, `ns`, and `zeroed_cts`
                     uint32_t index_offset = 0);

void compute_normals(const void* ti,              //  size = num_triangles * 3, element = uint32_t
                     uint32_t num_triangles,
                     const void* points,          //  points are 3 floats
                     void* ns,                    //  normals are 3 floats
                     void* zeroed_cts,            //  size = number of points, element = uint32_t
                     uint32_t index_offset = 0,
                     uint32_t point_stride = 0,   //  stride of vertex, 0 indicates densely packed.
                     uint32_t point_offset = 0,
                     uint32_t normal_stride = 0,
                     uint32_t normal_offset = 0);

void compute_normals_per_triangle(const uint32_t* ti,     //  size = num_triangles * 3
                                  uint32_t num_triangles,
                                  const Vec3f* points,    //  size = number of points
                                  Vec3f* ns);             //  size = num_triangles

Vec3f compute_normal(const Vec3f& p0, const Vec3f& p1, const Vec3f& p2);
bool is_ccw(const Vec3f& p0, const Vec3f& p1, const Vec3f& p2);
bool is_ccw(const uint32_t* tris, uint32_t num_tris, const Vec3f* ps);
bool is_ccw_or_zero(const uint32_t* tris, uint32_t num_tris, const Vec3f* ps);
void require_ccw(uint32_t* tris, uint32_t num_tris, const Vec3f* ps);
void require_ccw(void* tris, uint32_t num_tris, void* ps,
                 uint32_t p_stride, uint32_t p_offset, uint32_t index_offset);

//  Linear search for neighbor of `src` that shares the edge given by `ai` and `bi`. Returns ~0u if
//  no such neighbor.
uint32_t find_adjacent_order_independent(const uint32_t* tris, uint32_t num_triangles,
                                         uint32_t src, uint32_t ai, uint32_t bi);

constexpr uint32_t no_adjacent_triangle() {
  return ~0u;
}

uint32_t setdiff_edge(const uint32_t* tri, uint32_t ai, uint32_t bi);
void setdiff_point(const uint32_t* tri, uint32_t pi, uint32_t* ai, uint32_t* bi);
bool contains_point(const uint32_t* tri, uint32_t pi);

}