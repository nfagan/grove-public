#pragma once

#include "grove/math/vector.hpp"
#include "grove/common/Optional.hpp"
#include "types.hpp"
#include <functional>

namespace grove::geometry {

struct DistributeAlongAxisStep {
  int num_instances;
  Vec3f max_rotation;
  float radius;
  Vec3f scale;
  Vec2f scale_randomness_limits;
  Vec2f theta_randomness_limits;
};

struct DistributeAlongAxisParams {
  int num_steps;
  Vec3f step_axis;
  float step_length;
  Vec3f base_axis_offset;
  std::function<DistributeAlongAxisStep(int)> step;
};

struct DistributeAlongAxisBufferIndices {
  int pos_attr;
  Optional<int> norm_attr;
  Optional<int> uv_attr;
};

size_t distribute_along_axis(const void* in, const VertexBufferDescriptor& in_desc, size_t in_size,
                             const DistributeAlongAxisBufferIndices& in_indices,
                             void* out, const VertexBufferDescriptor& out_desc, size_t max_out_size,
                             const DistributeAlongAxisBufferIndices& out_indices,
                             const DistributeAlongAxisParams& params);

}