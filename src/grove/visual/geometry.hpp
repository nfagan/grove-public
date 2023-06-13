#pragma once

#include <vector>
#include <cstdint>
#include <array>

namespace grove {

template <typename T>
class ArrayView;

template <typename T>
struct Vec3;

}

namespace grove::geometry {

std::vector<float> triangle_strip_quad_positions(int vertex_count = 64, bool is_3d = true);
std::vector<float> triangle_strip_sphere_data(int vertex_count = 64,
                                              bool include_uv = true, bool include_normal = false);
std::vector<uint16_t> triangle_strip_indices(int vertex_count);

std::vector<float> segmented_quad_positions(int num_segments, bool is_3d = true);
void get_segmented_quad_positions(int num_segments, bool is_3d, float* out);

std::vector<float> quad_positions(bool is_3d = true, float z = 1.0f);
std::vector<float> xz_quad_positions(float y = 0.0f);
std::vector<uint16_t> quad_indices();

void get_quad_positions(float* dst, bool is_3d = true, float z = 1.0f);
void get_quad_indices(uint16_t* dst);

std::vector<float> cube_positions();
std::vector<uint16_t> cube_indices();

std::array<float, 6> full_screen_triangle_positions();

void make_normal_line_segment_positions(const ArrayView<const float>& positions,
                                        const ArrayView<const float>& normals,
                                        const ArrayView<const uint16_t>& indices,
                                        float line_length,
                                        std::vector<float>& out);

void push_aabb_line_segment_points(const Vec3<float>& p0,
                                   const Vec3<float>& p1,
                                   std::vector<Vec3<float>>& out);

}
