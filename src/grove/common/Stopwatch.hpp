#pragma once

#include <chrono>

namespace grove {

class Stopwatch {
public:
  using Clock = std::chrono::high_resolution_clock;
  using Delta = std::chrono::duration<double>;

  Delta delta() const {
    return Delta(Clock::now() - t0);
  }

  //  Calculate delta as (current time - last time), and set last time = current time.
  Delta delta_update() {
    auto now = Clock::now();
    auto d = Delta(now - t0);
    t0 = now;
    return d;
  }

  //  If delta() is greater than some epsilon (which is 0 by default),
  //  return 1.0 / delta(). Otherwise, return some default value.
  double rate(double dflt, double eps = 0.0) const {
    auto d = delta().count();
    return d > eps ? 1.0 / d : dflt;
  }

  double rate_update(double dflt, double eps = 0.0) {
    auto now = Clock::now();
    auto d = Delta(now - t0).count();
    t0 = now;
    return d > eps ? 1.0 / d : dflt;
  }

  void reset() {
    t0 = Clock::now();
  }

public:
  Clock::time_point t0{Clock::now()};
};

}