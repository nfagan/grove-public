#pragma once

#include "grove/common/Optional.hpp"

namespace grove {

class SoilComponent;

namespace soil {
struct ParameterModulator;
}

struct SoilGUIUpdateResult {
  Optional<bool> enabled;
  Optional<bool> parameter_capture_enabled;
  Optional<bool> lock_parameter_targets;
  Optional<bool> draw_texture;
  Optional<bool> overlay_player_position;
  Optional<float> overlay_radius;
  Optional<float> decay;
  Optional<float> diffuse_speed;
  Optional<bool> diffuse_enabled;
  Optional<bool> allow_perturb_event;
  Optional<float> time_scale;
  Optional<bool> circular_world;
  Optional<bool> only_right_turns;
  Optional<int> turn_speed_power;
  Optional<int> speed_power;
  bool close{};
};

class SoilGUI {
public:
  SoilGUIUpdateResult render(const SoilComponent& component,
                             const soil::ParameterModulator& soil_param_modulator);
};

}