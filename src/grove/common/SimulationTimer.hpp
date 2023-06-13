#pragma once

#include "Stopwatch.hpp"

namespace grove {

class SimulationTimer {
public:
  void on_frame_entry(double real_dt) {
    accumulated_time += real_dt;
  }

  bool should_proceed(double sim_dt) const {
    return accumulated_time >= sim_dt;
  }

  bool on_after_simulate_check_abort(double sim_dt, const Stopwatch& guard, double abort_threshold) {
    accumulated_time -= sim_dt;
    if (guard.delta().count() >= abort_threshold) {
      accumulated_time = 0;
      return true;
    } else {
      return false;
    }
  }

  double get_accumulated_time() const {
    return accumulated_time;
  }

private:
  double accumulated_time{};
};

}