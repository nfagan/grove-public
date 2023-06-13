#pragma once

#include "grove/gl/types.hpp"
#include <vector>

namespace grove {

class FrustumGrid;

enum class InstancePlacementPolicy {
  Random,
  AlternatingOffsets,
  AlternatingOffsets2,
  GoldenRatio
};

struct GrassInstanceOptions {
  int max_num_instances;
  float density;
  float next_density;
  InstancePlacementPolicy placement_policy;
  float placement_offset;
  float displacement_magnitude = 0.1f;
};

struct FrustumGridInstanceData {
  std::vector<float> data;
  int num_instances;
  VertexBufferDescriptor buffer_descriptor;
};

FrustumGridInstanceData
make_frustum_grid_instance_data(const FrustumGrid& grid, const GrassInstanceOptions& options);

}
