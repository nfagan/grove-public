#pragma once

#include "grove/math/vector.hpp"
#include "grove/math/Mat4.hpp"
#include <vector>

namespace grove {

class Camera;

}

namespace grove::csm {

struct CSMDescriptor {
  struct UVTransform {
    Vec3f scale{1.0f};
    Vec3f offset{};
  };

  int num_layers() const {
    return int(layer_z_offsets.size());
  }

  float ith_cascade_extent(int i) const {
    return layer_z_offsets[i].y - layer_z_offsets[i].x;
  }

  std::vector<Vec2f> layer_z_offsets;
  std::vector<Mat4f> light_space_view_projections;
  std::vector<UVTransform> uv_transforms;
  Mat4f light_shadow_sample_view;

  int texture_size;
  float layer_size;
  float layer_increment;
  float sign_y;
};

CSMDescriptor make_csm_descriptor(int num_layers,
                                  int texture_size,
                                  float layer_size,
                                  float layer_increment,
                                  float sign_y = 1.0f);
CSMDescriptor make_csm_descriptor(int num_layers,
                                  int texture_size,
                                  const float* layer_sizes,
                                  float sign_y = 1.0f);
void update_csm_descriptor(CSMDescriptor& descriptor,
                           const Camera& camera, const Vec3f& sun_position);

}