#pragma once

#include "grove/common/Optional.hpp"
#include "grove/math/vector.hpp"

namespace grove {

class ProceduralFlowerComponent;

class ProceduralFlowerGUI {
public:
  struct SetColors4 {
    Vec3<uint8_t> c0;
    Vec3<uint8_t> c1;
    Vec3<uint8_t> c2;
    Vec3<uint8_t> c3;
  };

public:
  struct UpdateResult {
    Optional<bool> render_attraction_points;
    Optional<bool> death_enabled;
    Optional<Vec2f> add_patch;
    Optional<float> patch_radius;
    Optional<int> patch_size;
    Optional<float> flower_stem_scale;
    Optional<float> flower_radius_power;
    Optional<float> flower_radius_scale;
    Optional<float> flower_radius_power_randomness;
    Optional<float> flower_radius_randomness;
    Optional<bool> randomize_flower_radius_scale;
    Optional<bool> randomize_flower_radius_power;
    Optional<float> axis_growth_incr;
    Optional<float> ornament_growth_incr;
    Optional<SetColors4> set_alpha_test_colors;
    Optional<uint32_t> selected_flower;
    Optional<bool> allow_bush;
    Optional<float> patch_position_radius;
    bool enable_randomization{};
    bool close{};
  };

  UpdateResult render(const ProceduralFlowerComponent& component);

private:
  Vec2f patch_position{};
};

}