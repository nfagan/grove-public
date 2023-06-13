#pragma once

#include "grove/math/vector.hpp"
#include "grove/math/OBB3.hpp"
#include "grove/common/memory.hpp"
#include "grove/common/Optional.hpp"
#include <vector>

namespace grove::arch {

struct TryEncirclePointParams {
  static TryEncirclePointParams make_default1(const float* piece_length);

  float dist_attract_until;
  float dist_begin_propel;
  float dt;
  const float* constant_speed;
  float attract_force_scale;
  float propel_force_scale;
  float attract_dist_falloff;
};

struct FitLineToPointsEntry {
  Vec2f p0;
  float dtheta;
};

struct FitLinesToPointsParams {
  float target_length;
  int max_num_fit;
  uint32_t p0_ind;
  Vec2f query_p;
  float f;
  float last_theta;
  LinearAllocator* result_entries;  //  FitLineToPointsEntry
};

struct FitBoundsToPointsContext {
  Vec3f p0;
  arch::FitLinesToPointsParams fit_params;
  arch::TryEncirclePointParams encircle_point_params;
  Vec2f line_v;
  Vec2f line_p;
  Vec2f line_target;
  std::vector<Vec2f> line_ps;
  int num_fit;
  std::unique_ptr<unsigned char[]> heap_data;
  LinearAllocator alloc;
};

void try_encircle_point(const Vec2f& target, const TryEncirclePointParams& params,
                        Vec2f* p, Vec2f* v);

int fit_line_to_points(const Vec2f* ps, uint32_t num_ps, FitLinesToPointsParams* params);

void initialize_fit_bounds_to_points_context(FitBoundsToPointsContext* context,
                                             const Vec3f& struct_ori,
                                             const Vec2f& line_target,
                                             const TryEncirclePointParams& encircle_point_params,
                                             int max_num_entries);
void initialize_fit_bounds_to_points_context_default(FitBoundsToPointsContext* context,
                                                     const Vec3f& struct_ori,
                                                     const Vec2f& line_target);

void set_line_target(FitBoundsToPointsContext* context, const Vec2f& line_target);
Optional<OBB3f> extrude_bounds(FitBoundsToPointsContext* context,
                               const Vec3f& size,
                               const OBB3f* parent_bounds);

}