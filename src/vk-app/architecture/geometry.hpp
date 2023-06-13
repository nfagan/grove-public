#pragma once

#include "common.hpp"
#include "grove/math/cdt.hpp"
#include <unordered_set>
#include <unordered_map>

namespace grove {
struct LinearAllocator;
}

namespace grove::arch {

struct GridCache {
  struct Entry {
    uint32_t point_offset;
    uint32_t num_points;
    uint32_t tri_offset;
    uint32_t num_tris;
  };

  std::unordered_map<uint64_t, Entry> entries;
  std::vector<Vec2<double>> points;
  std::vector<uint32_t> triangles;
};

struct GeometryAllocators {
  LinearAllocator* ps;
  LinearAllocator* ns;
  LinearAllocator* tris;
  LinearAllocator* tmp;
};

struct WallHole {
  static void push_default3(WallHole* result);
  float curl;
  Vec2f scale;
  Vec2f off;
  float rot;
};

//  connector points in inclusive ranges [x0_y0, x0_y1] and [x0_y0, x1_y1].
struct FaceConnectorIndices {
  uint32_t xi_size(uint32_t xi) const {
    assert(xi == 0 || xi == 1);
    return xi == 0 ? x0_size() : x1_size();
  }
  uint32_t xi_ith(uint32_t xi, uint32_t i) const {
    assert(xi == 0 || xi == 1);
    return xi == 0 ? ith_x0(i) : ith_x1(i);
  }
  uint32_t x0_size() const {
    assert(x0_y1 > x0_y0);
    return x0_y1 - x0_y0 + 1;
  }
  uint32_t ith_x0(uint32_t i) const {
    return i + x0_y0;
  }
  uint32_t x1_size() const {
    assert(x1_y1 > x1_y0);
    return x1_y1 - x1_y0 + 1;
  }
  uint32_t ith_x1(uint32_t i) const {
    return i + x1_y0;
  }
  void add_offset(uint32_t off) {
    x0_y0 += off;
    x0_y1 += off;
    x1_y0 += off;
    x1_y1 += off;
  }

  uint32_t x0_y0;
  uint32_t x0_y1;
  uint32_t x1_y0;
  uint32_t x1_y1;
};

struct WallHoleResult {
  std::vector<cdt::Triangle> triangles;
  std::vector<Vec3f> positions;
  std::vector<Vec3f> normals;
  std::unordered_set<uint32_t> interior_edge_points;
  uint32_t bot_l_ind{};
  uint32_t bot_r_ind{};
  uint32_t top_r_ind{};
  uint32_t top_l_ind{};
};

struct WallHoleParams {
  const WallHole* holes{nullptr};
  uint32_t num_holes{};
  int straight_hole_x_segments{10};
  int curved_hole_x_segments{5};
  int hole_y_segments{5};
  int grid_x_segments{10};
  int grid_y_segments{10};
  int dim_perm[3]{0, 1, 2};
  float aspect_ratio{1.0f};
};

struct StraightFlatSegmentParams {
  int grid_x_segments{10};
  int grid_y_segments{10};
  int dim_perm[3]{0, 1, 2};
};

struct TriangulationResult {
  std::vector<cdt::Triangle> triangles;
  std::vector<Vec3f> positions;
  std::vector<Vec3f> normals;
};

struct AdjoiningCurvedSegmentParams {
  TriangulatedGrid grid;
  Vec2f p0;
  Vec2f p1;
  Vec2f v0;
  Vec2f v1;
  Vec2f n0;
  Vec2f n1;
  GeometryAllocators alloc;
  uint32_t index_offset;
  float y_scale;
  float y_offset;
  uint32_t* num_points_added;
  uint32_t* num_indices_added;
  arch::FaceConnectorIndices* negative_x;
  arch::FaceConnectorIndices* positive_x;
};

struct WallParams {
  OBB3f bounds;
  uint32_t base_index_offset;

  const Vec3f* wall_ps;
  const Vec3f* wall_ns;
  uint32_t num_wall_points;
  const uint32_t* wall_tris;
  uint32_t num_wall_tris;
  const std::unordered_set<uint32_t>* wall_interior_inds;
  uint32_t wall_bot_l_ind;
  uint32_t wall_bot_r_ind;
  uint32_t wall_top_r_ind;
  uint32_t wall_top_l_ind;

  const Vec3f* flat_ps;
  const Vec3f* flat_ns;
  uint32_t num_flat_points;
  const uint32_t* flat_tris;
  uint32_t num_flat_tris;

  GeometryAllocators alloc;
  uint32_t* num_points_added;
  uint32_t* num_indices_added;
  FaceConnectorIndices* positive_x;
  FaceConnectorIndices* negative_x;
};

struct CurvedVerticalConnectionParams {
  OBB3f bounds;
  TriangulatedGrid xy;
  TriangulatedGrid xz;
  float min_y;
  bool target_lower;
  float power;
  uint32_t index_offset;
  GeometryAllocators alloc;
  uint32_t* num_points_added;
  uint32_t* num_indices_added;
};

struct ArchWallParams {
  OBB3f bounds;
  TriangulatedGrid arch_xz;
  TriangulatedGrid arch_yz;
  TriangulatedGrid straight_yz;
  TriangulatedGrid straight_xz;
  float outer_radius;
  float inner_radius;
  float side_additional_width;
  float side_additional_width_power;
  float straight_length_scale;
  float width;
  uint32_t index_offset;
  GeometryAllocators alloc;
  uint32_t* num_points_added;
  uint32_t* num_indices_added;
};

struct PoleParams {
  OBB3f bounds;
  TriangulatedGrid grid;
  uint32_t index_offset;
  GeometryAllocators alloc;
  uint32_t* num_points_added;
  uint32_t* num_indices_added;
};

void require_triangulated_grid(GridCache* cache, int w, int h);
TriangulatedGrid acquire_triangulated_grid(const GridCache* cache, int w, int h);
TriangulatedGrid make_triangulated_grid(const std::vector<cdt::Triangle>& tris,
                                        const std::vector<cdt::Point>& points);
GeometryAllocators make_geometry_allocators(LinearAllocator* ps, LinearAllocator* ns,
                                            LinearAllocator* tris, LinearAllocator* tmp);
void clear_geometry_allocators(const GeometryAllocators* alloc);

WallHoleResult make_wall_hole(const WallHoleParams& params);
TriangulationResult make_straight_flat_segment(const StraightFlatSegmentParams& params);
void make_adjoining_curved_segment(const AdjoiningCurvedSegmentParams& params);
void make_curved_vertical_connection(const CurvedVerticalConnectionParams& params);
void make_arch_wall(const ArchWallParams& params);
void make_wall(const WallParams& params);
void make_pole(const PoleParams& params);
WallParams make_wall_params(const OBB3f& wall_bounds, uint32_t base_index_offset,
                            const WallHoleResult& hole_res,
                            const TriangulationResult& seg_res,
                            GeometryAllocators alloc,
                            uint32_t* num_points_added,
                            uint32_t* num_indices_added,
                            FaceConnectorIndices* positive_x,
                            FaceConnectorIndices* negative_x);

void truncate_to_uint16(const uint32_t* src, uint16_t* dst, size_t num_indices);

}