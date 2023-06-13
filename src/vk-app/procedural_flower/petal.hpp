#pragma once

#include "grove/math/vector.hpp"

namespace grove::petal {

struct ShapeParams {
  static ShapeParams lilly(float growth_frac, float radius_scale = 1.0f);
  static ShapeParams alla(float growth_frac, float radius_scale = 5.0f);
  static ShapeParams tulip(float growth_frac, float radius_scale = 5.0f);
  static ShapeParams plane(float growth_frac, float death_frac,
                           float radius_scale = 1.0f, float radius_power = 5.0f);

  float min_radius;
  float radius;
  float radius_power;

  union {
    float max_additional_radius;
    float mix_texture_color;
  };

  float circumference_frac0;
  float circumference_frac1;
  float circumference_frac_power;
  float curl_scale;

  Vec2f scale;
  float group_frac;

  union {
    float max_negative_y_offset;
    float min_z_discard_enabled;
  };
};

struct MaterialParams {
  static int random_perm_index();
  static MaterialParams type0(int pi = -1);
  static MaterialParams type1(int pi = -1);
  static MaterialParams type2(int pi = -1);
  static MaterialParams type3(int pi = -1);
  static Vec3<int> component_indices_from_perm_index(int pi);

  Vec4f color_info0;
  Vec3<int> color_component_indices;
};

}