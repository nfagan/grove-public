#pragma once

#include "grove/math/Vec3.hpp"

namespace grove::foliage {

struct FoliageDistributionParams {
  int num_steps;
  int num_instances_per_step;
  Vec3f outwards_direction;
  Vec3f tip_position;
  float translation_log_min_x;
  float translation_log_max_x;
  float translation_step_power;
  float translation_step_spread_scale;
  float translation_x_scale;
  float translation_y_scale;
  float rand_z_rotation_scale;
};

struct FoliageDistributionEntry {
  Vec3f translation;
  Vec3f right_dir;
  Vec3f forwards_dir;
  float y_rotation;
  float z_rotation;
  float randomness;
};

//  number of dst_entries >= num_steps * instances_per_step
uint32_t distribute_foliage_outwards_from_nodes(const FoliageDistributionParams& params,
                                                FoliageDistributionEntry* dst_entries);

}