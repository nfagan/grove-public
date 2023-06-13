#pragma once

#include "common.hpp"

namespace grove {

struct WeatherSystemImpl;

class WeatherSystem {
public:
  WeatherSystem();
  ~WeatherSystem();
  weather::Status update();

  void set_stationary_time(double t);
  double get_stationary_time() const;
  void set_update_enabled(bool st);
  void set_immediate_state(weather::State state);
  void begin_transition();
  void set_frac_next_state(float v);
  bool get_update_enabled() const;
  const weather::Status& get_status() const;

private:
  WeatherSystemImpl* impl;
};

}