#pragma once

#include "grove/math/vector.hpp"

namespace grove {
class Terrain;
}

namespace grove::fog {

struct TransientMistElement {
  Vec3f normalized_translation;
  Vec3f position;
  float opacity;
  float elapsed_time;
  float total_time;
  bool elapsed;
};

struct TransientMistTickParams {
  const Vec3f* camera_position;
  const Vec3f* camera_right;
  const Vec3f* camera_forward;
  const Terrain* terrain;
  float y_offset;
  float real_dt;
  float grid_size;
  float dist_begin_attenuation;
  Vec2f camera_front_distance_limits;
  Vec2f camera_right_distance_limits;
};

void distribute_transient_mist_elements(TransientMistElement* elements, int num_elements);
void tick_transient_mist(TransientMistElement* elements, int num_elements,
                         const TransientMistTickParams& params);

}