#pragma once

#include "grove/common/Optional.hpp"
#include "../sky/SkyGradient.hpp"

namespace grove {

class SkyComponent;

struct SkyGUIUpdateResult {
  Optional<bool> weather_controls_gradient;
  bool use_default_sun{};
  Optional<bool> use_sun_angles;
  Optional<SkyGradient::Params> sky_gradient_params;
  Optional<float> sun_position_theta01;
  Optional<float> sun_position_phi_radians;
  bool close{};
};

class SkyGUI {
public:
  SkyGUIUpdateResult render(const SkyComponent& component);
};

}