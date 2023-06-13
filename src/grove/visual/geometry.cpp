#include "geometry.hpp"
#include "grove/common/common.hpp"
#include "grove/common/ArrayView.hpp"
#include "grove/math/constants.hpp"
#include "grove/math/vector.hpp"
#include <cmath>

GROVE_NAMESPACE_BEGIN

std::vector<uint16_t> geometry::triangle_strip_indices(int vertex_count) {
  std::vector<uint16_t> result;
  
  uint16_t first_index = 0;
  auto next_index = uint16_t(vertex_count);
  uint16_t index_stp = 0;
  bool should_proceed = true;
  
  while (should_proceed) {
    result.push_back(first_index);
    result.push_back(next_index);
    index_stp += 2;
    
    should_proceed = next_index != (vertex_count * vertex_count) - 1;
    
    if (index_stp > 0 && (next_index+1) % vertex_count == 0 && should_proceed) {
      result.push_back(next_index);
      result.push_back(first_index+1);
      index_stp += 2;
    }
    
    first_index++;
    next_index++;
  }
  
  return result;
}

std::vector<float> geometry::triangle_strip_quad_positions(int vertex_count, bool is_3d) {
  std::vector<float> result;
  
  const int num_components = is_3d ? 3 : 2;
  result.resize(vertex_count * vertex_count * num_components);
  
  int index = 0;
  const auto denom = float(vertex_count - 1);
  
  for (int i = 0; i < vertex_count; i++) {
    for (int j = 0; j < vertex_count; j++) {
      const float x_segment = float(j) / denom;
      const float z_segment = float(i) / denom;
      
      result[index++] = x_segment;
      
      if (is_3d) {
        result[index++] = 0.0f;
      }
      
      result[index++] = z_segment;
    }
  }
  
  return result;
}

std::vector<float> geometry::triangle_strip_sphere_data(int vertex_count,
                                                        bool include_uv,
                                                        bool include_normal) {
  std::vector<float> result;
  
  int vertex_size = 3;
  
  if (include_uv) {
    vertex_size += 2;
  }

  if (include_normal) {
    vertex_size += 3;
  }
  
  result.resize(vertex_count * vertex_count * vertex_size);
  
  int index = 0;
  const float denom = float(vertex_count - 1);
  constexpr float pif = float(grove::pi());
  
  for (int i = 0; i < vertex_count; i++) {
    for (int j = 0; j < vertex_count; j++) {
      const float x_segment = float(j) / denom;
      const float y_segment = float(i) / denom;
      
      const float sin_y_segment = std::sin(y_segment * pif);
      const float x = std::cos(x_segment * 2.0f * pif) * sin_y_segment;
      const float y = std::cos(y_segment * pif);
      const float z = std::sin(x_segment * 2.0f * pif) * sin_y_segment;
      
      result[index++] = x;
      result[index++] = y;
      result[index++] = z;

      if (include_normal) {
        result[index++] = x;
        result[index++] = y;
        result[index++] = z;
      }
      
      if (include_uv) {
        result[index++] = x_segment;
        result[index++] = y_segment;
      }
    }
  }
  
  return result;
}

void geometry::get_segmented_quad_positions(int num_segments, bool is_3d, float* out) {
  const float segment_size = 1.0f / float(num_segments);

  int j{};
  for (int i = 0; i < num_segments; i++) {
    const float x0 = -1.0f;
    const float x1 = 1.0f;
    const float y0 = float(i) * segment_size;
    const float y1 = y0 + segment_size;
    const float z = 0.0f;

    //  tri1.
    out[j++] = x0;
    out[j++] = y1;
    if (is_3d) {
      out[j++] = z;
    }

    out[j++] = x0;
    out[j++] = y0;
    if (is_3d) {
      out[j++] = z;
    }

    out[j++] = x1;
    out[j++] = y0;
    if (is_3d) {
      out[j++] = z;
    }

    //  tri2.
    out[j++] = x1;
    out[j++] = y0;
    if (is_3d) {
      out[j++] = z;
    }

    out[j++] = x1;
    out[j++] = y1;
    if (is_3d) {
      out[j++] = z;
    }

    out[j++] = x0;
    out[j++] = y1;
    if (is_3d) {
      out[j++] = z;
    }
  }
}

std::vector<float> geometry::segmented_quad_positions(int num_segments, bool is_3d) {
  const float segment_size = 1.0f / num_segments;
  std::vector<float> positions;
  
  for (int i = 0; i < num_segments; i++) {
    const float x0 = -1.0f;
    const float x1 = 1.0f;
    const float y0 = float(i) * segment_size;
    const float y1 = y0 + segment_size;
    const float z = 0.0f;
    
    //  tri1.
    positions.push_back(x0);
    positions.push_back(y1);
    
    if (is_3d) {
      positions.push_back(z);
    }
    
    positions.push_back(x0);
    positions.push_back(y0);
    
    if (is_3d) {
      positions.push_back(z);
    }
    
    positions.push_back(x1);
    positions.push_back(y0);
    
    if (is_3d) {
      positions.push_back(z);
    }
    
    //  tri2.
    positions.push_back(x1);
    positions.push_back(y0);
    
    if (is_3d) {
      positions.push_back(z);
    }
    
    positions.push_back(x1);
    positions.push_back(y1);
    
    if (is_3d) {
      positions.push_back(z);
    }
    
    positions.push_back(x0);
    positions.push_back(y1);
    
    if (is_3d) {
      positions.push_back(z);
    }
  }
  
  return positions;
}

std::vector<uint16_t> geometry::quad_indices() {
  return std::vector<uint16_t>{0, 1, 2, 0, 2, 3};
}

std::vector<float> geometry::quad_positions(bool is_3d, float z) {
  if (is_3d) {
    return std::vector<float>{
      -1.0, -1.0, z,
      1.0, -1.0, z,
      1.0, 1.0, z,
      -1.0, 1.0, z
    };
  } else {
    return std::vector<float>{
      -1.0, -1.0,
      1.0, -1.0,
      1.0,  1.0,
      -1.0,  1.0,
    };
  }
}

void geometry::get_quad_indices(uint16_t* dst) {
  *dst++ = 0; *dst++ = 1; *dst++ = 2;
  *dst++ = 0; *dst++ = 2; *dst++ = 3;
}

void geometry::get_quad_positions(float* dst, bool is_3d, float z) {
  if (is_3d) {
    *dst++ = -1.0f; *dst++ = -1.0f; *dst++ = z;
    *dst++ = 1.0f; *dst++ = -1.0f; *dst++ = z;
    *dst++ = 1.0f; *dst++ = 1.0f; *dst++ = z;
    *dst++ = -1.0f; *dst++ = 1.0f; *dst++ = z;
  } else {
    *dst++ = -1.0f; *dst++ = -1.0f;
    *dst++ = 1.0f; *dst++ = -1.0f;
    *dst++ = 1.0f; *dst++ = 1.0f;
    *dst++ = -1.0f; *dst++ = 1.0f;
  }
}

std::vector<float> geometry::xz_quad_positions(float y) {
  return std::vector<float>{
    -1.0, y, -1.0,
    1.0, y, -1.0,
    1.0, y, 1.0,
    -1.0, y, 1.0,
  };
}

std::vector<float> geometry::cube_positions() {
  return std::vector<float>{
    -1.0, -1.0, 1.0,
    1.0, -1.0, 1.0,
    1.0,  1.0, 1.0,
    -1.0, 1.0, 1.0,

    -1.0, -1.0, -1.0,
    -1.0,  1.0, -1.0,
    1.0,  1.0, -1.0,
    1.0, -1.0, -1.0,

    -1.0,  1.0, -1.0,
    -1.0,  1.0,  1.0,
    1.0,  1.0,  1.0,
    1.0,  1.0, -1.0,

    -1.0, -1.0, -1.0,
    1.0, -1.0, -1.0,
    1.0, -1.0,  1.0,
    -1.0, -1.0,  1.0,

    1.0, -1.0, -1.0,
    1.0,  1.0, -1.0,
    1.0,  1.0,  1.0,
    1.0, -1.0,  1.0,

    -1.0, -1.0, -1.0,
    -1.0, -1.0,  1.0,
    -1.0,  1.0,  1.0,
    -1.0,  1.0, -1.0,
  };
}

std::vector<uint16_t> geometry::cube_indices() {
  return std::vector<uint16_t>{
    0,  1,  2,      0,  2,  3,
    4,  5,  6,      4,  6,  7,
    8,  9,  10,     8,  10, 11,
    12, 13, 14,     12, 14, 15,
    16, 17, 18,     16, 18, 19,
    20, 21, 22,     20, 22, 23,
  };
}

std::array<float, 6> geometry::full_screen_triangle_positions() {
  return std::array<float, 6>{{-1.0f, -1.0f, -1.0f, 3.0f, 3.0f, -1.0f}};
}

void geometry::make_normal_line_segment_positions(const ArrayView<const float>& positions,
                                                  const ArrayView<const float>& normals,
                                                  const ArrayView<const uint16_t>& indices,
                                                  float line_length, std::vector<float>& out) {
  const auto make_vec3 = [&](int i, auto&& data) {
    const auto i0 = indices[i] * 3;
    return Vec3f{data[i0], data[i0 + 1], data[i0 + 2]};
  };

  for (int i = 0; i < int(indices.size()) / 3; i++) {
    for (int j = 0; j < 3; j++) {
      auto p0 = make_vec3(i * 3 + j, positions);
      auto n0 = make_vec3(i * 3 + j, normals);
      auto p1 = p0 + n0 * line_length;

      for (int k = 0; k < 3; k++) {
        out.push_back(p0[k]);
      }
      for (int k = 0; k < 3; k++) {
        out.push_back(p1[k]);
      }
    }
  }
}

void geometry::push_aabb_line_segment_points(const Vec3<float>& p0,
                                             const Vec3<float>& p1,
                                             std::vector<Vec3<float>>& out) {
  auto face00 = p0;
  auto face10 = Vec3f{p1.x, p0.y, p0.z};
  auto face01 = Vec3f{p0.x, p1.y, p0.z};
  auto face11 = Vec3f{p1.x, p1.y, p0.z};

  std::array<Vec3f, 4> faces0{{face00, face10, face01, face11}};
  std::array<Vec3f, 4> faces1{};
  for (int i = 0; i < 4; i++) {
    faces1[i] = Vec3f{faces0[i].x, faces0[i].y, p1.z};
  }

  out.push_back(faces0[0]);
  out.push_back(faces0[1]);
  out.push_back(faces0[1]);
  out.push_back(faces0[3]);
  out.push_back(faces0[3]);
  out.push_back(faces0[2]);
  out.push_back(faces0[0]);
  out.push_back(faces0[2]);

  out.push_back(faces1[0]);
  out.push_back(faces1[1]);
  out.push_back(faces1[1]);
  out.push_back(faces1[3]);
  out.push_back(faces1[3]);
  out.push_back(faces1[2]);
  out.push_back(faces1[0]);
  out.push_back(faces1[2]);

  out.push_back(faces0[0]);
  out.push_back(faces1[0]);
  out.push_back(faces0[1]);
  out.push_back(faces1[1]);
  out.push_back(faces0[2]);
  out.push_back(faces1[2]);
  out.push_back(faces0[3]);
  out.push_back(faces1[3]);
}

GROVE_NAMESPACE_END
