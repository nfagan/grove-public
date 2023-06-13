#pragma once

#include "grove/math/OBB3.hpp"
#include "grove/math/Bounds3.hpp"
#include "grove/math/Mat3.hpp"

namespace grove::tree {
struct Internode;
}

namespace grove::bounds {

struct FitOBBsAroundAxisParams {
  enum class TestType {
    None = 0,
    SizeRatio,
    MaxSize
  };

  const OBB3f* axis_bounds;
  int num_bounds;
  Vec3f max_size_ratio;
  Vec3f max_size;
  TestType test_type;
  Vec3f preferred_axis;
  bool use_preferred_axis;
  OBB3f* dst_bounds;
};

int fit_obbs_around_axis(const FitOBBsAroundAxisParams& params);

int fit_aabbs_around_axes_radius_threshold_method(
  const tree::Internode* nodes, const Mat3f* node_frames, int num_nodes,
  int min_medial, int max_medial, float xz_thresh,
  Bounds3f* dst_bounds, int* assigned_to_bounds);

int fit_aabbs_around_axes_only_medial_children_method(
  const tree::Internode* nodes, int num_nodes, int interval,
  Bounds3f* dst_bounds, int* assigned_to_bounds);

}