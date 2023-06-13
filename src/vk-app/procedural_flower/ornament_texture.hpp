#pragma once

#include "grove/math/vector.hpp"
#include <vector>

namespace grove::image {

struct Descriptor;

struct PetalTransform {
  float theta;
  float scale;
  float offset;
};

struct PetalShape1Params {
  static PetalShape1Params make_debug1();

  int num_curve_pts;
  int filter_win_size;
  float filter_noise_scale;
  float x_scale;
  float y_scale;
  float petal_rand_scale;
  float petal_radial_off;
  float petal_phase_off;
  int num_petals;
};

struct PetalShape1Result {
  std::vector<std::vector<Vec2f>> p_sets;
  std::vector<std::vector<Vec2f>> n_sets;
  std::vector<PetalTransform> petal_transforms;
};

PetalShape1Result petal_shape1_pipeline(const PetalShape1Params& params);

struct LineSplotchMaskParams {
  static LineSplotchMaskParams make_default();

  int num_line_points;
  int num_filter_points;
  int num_reps;
  float space;
  Vec2f off;
  float line_noise_scale;
  float rot_frac;
  float expand;
  float expand_off;
};

void make_default_line_distance_mask(const PetalShape1Result& shape_result,
                                     int rows, int cols,
                                     std::vector<float>* shape,
                                     std::vector<float>* distance,
                                     std::vector<int>* set_index);

void make_default_line_splotch_mask(const LineSplotchMaskParams& params,
                                    int rows, int cols, std::vector<float>* mask);

struct PetalTextureMaterial1Params {
  float* dst;
  const Descriptor* dst_desc;

  const float* petal_shape;
  const Descriptor* petal_shape_desc;

  const float* distance;
  const Descriptor* distance_desc;
  float distance_power;

  const int* petal_set_index;
  const Descriptor* petal_set_desc;

  const float* base_color_mask;
  const Descriptor* base_color_desc;

  const float* center_color_mask;
  const Descriptor* center_color_desc;
  float center_color_scale;

  const float* center_base_mask;
  const Descriptor* center_base_desc;

  const PetalTransform* petal_transforms;
  int num_petal_transforms;
};

void petal_texture_material1(const PetalTextureMaterial1Params& params);
void apply_petal_texture_material(const float* src, const Descriptor& src_desc,
                                  const Vec3f& color0, const Vec3f& color1,
                                  const Vec3f& color2, const Vec3f& color3,
                                  bool to_srgb, float* dst);

}