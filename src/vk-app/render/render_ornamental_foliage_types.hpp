#pragma once

#include "grove/math/vector.hpp"

namespace grove::foliage {

struct OrnamentalFoliageSmallInstanceData {
  Vec4f translation_direction_x;
  Vec4f direction_yz_unused;

  union {
    float min_radius;
    float aspect;
  };
  union {
    float radius;
    float scale;
  };
  union {
    float radius_power;
    float y_rotation_theta;
  };
  float curl_scale;
  float tip_y_fraction;
  float world_origin_x;
  float world_origin_z;

  uint32_t texture_layer_index;
  uint32_t color0;
  uint32_t color1;
  uint32_t color2;
  uint32_t color3;
};

struct OrnamentalFoliageLargeInstanceAggregateData {
  Vec4f aggregate_aabb_p0;
  Vec4f aggregate_aabb_p1;
};

struct OrnamentalFoliageLargeInstanceData {
  Vec4f translation_direction_x;
  Vec4f direction_yz_unused;
  union {
    float min_radius;
    float aspect;
  };
  union {
    float radius;
    float scale;
  };
  union {
    float radius_power;
    float y_rotation_theta;
  };
  float curl_scale;
  uint32_t texture_layer_index;
  uint32_t color0;
  uint32_t color1;
  uint32_t color2;
  uint32_t color3;
  uint32_t aggregate_index;
  uint32_t pad1;
  uint32_t pad2;
  Vec4<uint32_t> wind_info0;
  Vec4<uint32_t> wind_info1;
  Vec4<uint32_t> wind_info2;
};

}