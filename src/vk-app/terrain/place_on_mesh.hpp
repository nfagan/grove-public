#pragma once

#include "grove/math/vector.hpp"
#include "grove/math/Bounds2.hpp"
#include "grove/math/Mat3.hpp"
#include "grove/math/OBB3.hpp"

namespace grove::mesh {

void rasterize_bounds(const Bounds2f* bounds, const float* zs, int num_bounds,
                      int rows, int cols, int* im, float* depths);

struct GenOBB3OriginDirectionResult {
  bool success;
  Vec3f p;
  Vec3f n;
  Mat3f frame;
};

struct GenOBB3OriginDirectionParams {
  Vec2f image_sample_center_position;
  Vec2f image_sample_size;
  const Vec2f* image_sample_positions;
  int num_samples;

  const uint32_t* tris;
  const Vec3f* ps;
  const Vec3f* ns;

  const int* ti_im;
  int ti_im_rows;
  int ti_im_cols;
};

GenOBB3OriginDirectionResult gen_obb3_origin_direction(const GenOBB3OriginDirectionParams& params);

struct PlacePointsWithinOBB3Entry {
  Vec3f position;
  int obb3_index;
};

struct PlacePointsWithinOBB3Params {
  const uint32_t* tris;
  uint32_t num_tris;
  const Vec3f* ps;

  Vec3f surface_p;
  Mat3f obb3_frame;
  Vec3f obb3_size;

  const Vec2f* sample_positions;
  int num_samples;

  PlacePointsWithinOBB3Entry* result_entries;
};

struct PlacePointsWithinOBB3Result {
  int num_hits;
  float min_ray_t;
  float max_ray_t;
};

PlacePointsWithinOBB3Result place_points_within_obb3(const PlacePointsWithinOBB3Params& params);
OBB3f gen_obb3(const Vec3f& surface_p, const Mat3f& frame, const Vec3f& size,
               float ray_min_t, float ray_max_t);

void project_vertices_to_aabbs(const uint32_t* tris, uint32_t num_tris, const Vec3f* ps,
                               uint32_t num_ps, const Vec3f& cube_face_normal, Bounds2f* dst_bounds,
                               float* collapsed_depths);

}