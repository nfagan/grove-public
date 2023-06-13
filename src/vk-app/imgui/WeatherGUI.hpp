#pragma once

#include "grove/common/Optional.hpp"

namespace grove {

class WeatherComponent;

struct WeatherGUIUpdateResult {
  Optional<bool> update_enabled;
  Optional<bool> set_sunny;
  Optional<bool> set_overcast;
  Optional<float> set_frac_next;
  Optional<float> rain_alpha_scale;
  Optional<float> manual_rain_alpha_scale;
  Optional<bool> immediate_transition;
  bool close{};
};

class WeatherGUI {
public:
  WeatherGUIUpdateResult render(WeatherComponent& component);
};

}