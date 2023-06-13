#pragma once

#include "grove/visual/types.hpp"
#include "grove/math/vector.hpp"
#include <vector>

namespace grove::tree {

struct LeafGeometryResult {
  std::vector<float> data;
  VertexBufferDescriptor descriptor;
};

struct LeafGeometryParams {
  static LeafGeometryParams make_original();
  static LeafGeometryParams make_flattened();

  Vec3f step_scale;
  float tip_radius;
  float tip_radius_power;
};

LeafGeometryResult make_planes_distributed_along_axis(const LeafGeometryParams& params);

}