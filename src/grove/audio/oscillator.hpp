#pragma once

#include "types.hpp"
#include "grove/math/constants.hpp"
#include <array>
#include <atomic>
#include <cmath>

namespace grove {

namespace osc {

namespace detail {

inline void iterative_wrap_phase(double* phase, double period) {
  double current_phase = *phase;
  while (current_phase >= period) {
    current_phase -= period;
  }
  while (current_phase < 0) {
    current_phase += period;
  }
  *phase = current_phase;
}

inline void increment_phase(double& current_phase, double incr, double period) {
  current_phase += incr;

  while (current_phase >= period) {
    current_phase -= period;
  }

  while (current_phase < 0) {
    current_phase += period;
  }
}

} //  osc::detail

/*
 * Sin
 */

class Sin {
public:
  Sin();
  explicit Sin(double sample_rate);
  Sin(double sample_rate, double frequency, double current_phase = 0.0);

  Sample tick();
  Sample current() const;

  void set_frequency(double to);
  double get_frequency() const;

  void set_sample_rate(double to);

  static inline double tick(double sample_rate, double* phase, double freq) {
    auto val = std::sin(*phase);
    const auto incr = grove::two_pi() / sample_rate * freq;
    detail::increment_phase(*phase, incr, grove::two_pi());
    return val;
  }

private:
  double period_over_sr;
  double current_phase;
  double frequency;
};

/*
 * WaveTable
 */

class WaveTable {
public:
  static constexpr int size = 1024;

public:
  WaveTable();
  WaveTable(double sample_rate, double frequency);

  void set_sample_rate(double to);
  void set_frequency(double to);
  double get_frequency() const;

  void fill_sin();
  void fill_square(int num_harms);
  void fill_tri(int num_harms);
  void fill_white_noise();
  [[nodiscard]] bool fill_samples(const Sample* samples, int count);

  void normalize();

  Sample tick();
  Sample read(double phase) const;

private:
  double period_over_sr;
  double current_phase;
  double frequency;

private:
  std::array<Sample, size+1> table;
};

/*
 * WaveTable impl.
 */

inline void WaveTable::set_frequency(double to) {
  frequency = to;
}

inline void WaveTable::set_sample_rate(double to) {
  period_over_sr = double(size) / to;
}

inline Sample WaveTable::read(double phase) const {
  const int index = int(phase);
  assert(index >= 0 && index < size);
  const double frac = phase - index;
  const auto x0 = table[index];
  const auto x1 = table[index + 1];
  return Sample((1.0 - frac) * x0 + frac * x1);
}

inline Sample WaveTable::tick() {
  const int index = int(current_phase);
  const double frac = current_phase - index;
  const auto x0 = table[index];
  const auto x1 = table[index + 1];
  const auto sample = (1.0 - frac) * x0 + frac * x1;

  detail::increment_phase(current_phase, period_over_sr * frequency, double(size));

  return Sample(sample);
}

/*
 * Sin impl.
 */

inline Sample Sin::tick() {
  auto val = std::sin(current_phase);
  const auto incr = period_over_sr * frequency;
  detail::increment_phase(current_phase, incr, grove::two_pi());
  return Sample(val);
}

inline Sample Sin::current() const {
  return Sample(std::sin(current_phase));
}

inline void Sin::set_frequency(double to) {
  frequency = to;
}

inline double Sin::get_frequency() const {
  return frequency;
}

inline void Sin::set_sample_rate(double sample_rate) {
  period_over_sr = grove::two_pi() / sample_rate;
}

}

}