#pragma once

#include "grove/common/Optional.hpp"
#include "../transform/trs.hpp"

namespace grove {

class FogComponent;

struct FogGUIUpdateResult {
  bool close{};
  bool recompute_noise{};
  bool make_fog{};
  Optional<TRS<float>> new_transform_source;
  Optional<bool> depth_test_enabled;
  Optional<bool> wind_influence_enabled;
  Optional<float> wind_influence_scale;
  Optional<Vec3f> uvw_scale;
  Optional<Vec3f> uvw_offset;
  Optional<Vec3f> color;
  Optional<float> density;
  Optional<bool> manual_density;
  Optional<bool> billboard_depth_test_enabled;
  Optional<TRS<float>> billboard_transform_source;
  Optional<float> billboard_opacity_scale;
};

class FogGUI {
public:
  FogGUIUpdateResult render(const FogComponent& component);
};

}