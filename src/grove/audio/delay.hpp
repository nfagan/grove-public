#pragma once

#include "oscillator.hpp"
#include "grove/math/util.hpp"
#include "grove/common/util.hpp"
#include <memory>

//  Suppress double -> float warnings.
#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4244 )
#endif

namespace grove::audio {

template <typename Sample>
class SimpleDelayLine {
public:
  SimpleDelayLine() = default;
  explicit SimpleDelayLine(int size) : buffer{std::make_unique<Sample[]>(size)}, size{size} {
    //
  }

  Sample current() const {
    return index >= size ? Sample{} : buffer[index];
  }

  void tick(Sample input) {
    if (index >= size) {
      return;
    } else {
      buffer[index] = input;
      index = (index + 1) % size;
    }
  }

private:
  std::unique_ptr<Sample[]> buffer;
  int index{0};
  int size{};
};

template <typename Sample>
class InterpolatedDelayLine {
public:
  InterpolatedDelayLine() = default;

  explicit InterpolatedDelayLine(int max_size) :
    buffer{std::make_unique<Sample[]>(max_size)},
    max_size{max_size} {
    //
  }

  InterpolatedDelayLine(double sr, double max_time) :
    InterpolatedDelayLine(std::max(0, int(std::floor(sr * max_time)))) {
    //
  }

  Sample tick(Sample input, double delay_time, double sr, double feedback = 0.0);

private:
  std::unique_ptr<Sample[]> buffer;
  int max_size{};
  int wp{};
};

template <typename Sample>
inline Sample InterpolatedDelayLine<Sample>::tick(Sample input, double delay_time,
                                                  double sr, double feedback) {
  if (wp >= max_size) {
    return Sample{};
  }

  auto delay_samples = std::max(0.0, delay_time * sr);
  auto delay_frames = int(delay_samples);
  auto frac_frame = delay_samples - delay_frames;
  delay_frames = std::min(delay_frames, max_size);

  auto rp0 = wrap_within_range(wp - delay_frames, max_size);
  auto rp1 = (rp0 + 1) % max_size;

  const Sample delayed = grove::lerp(frac_frame, buffer[rp0], buffer[rp1]);
  const auto res = input + delayed * feedback;

  buffer[wp] = res;
  wp = (wp + 1) % max_size;

  return delayed;
}

template <typename Sample>
class ModulatedDelayLine {
public:
  ModulatedDelayLine() = default;
  explicit ModulatedDelayLine(double sample_rate,
                              double max_delay_time,
                              double center_delay_time,
                              double lfo_modulation_time,
                              double lfo_frequency,
                              double lfo_phase_offset = 0.0);

  Sample tick(Sample s, double sr, double feedback);
  void change_sample_rate(double to);
  void set_lfo_frequency(double freq) {
    lfo.set_frequency(freq);
  }

  void set_center_delay_time(double dt) {
    center_delay_time = std::max(0.0, std::min(dt, max_delay_time));
  }

  double get_current_lfo_value() const {
    return lfo.current();
  }

private:
  int wp{};
  int buffer_size{};
  std::unique_ptr<Sample[]> buffer;

  double max_delay_time{};
  double center_delay_time{};
  double lfo_modulation_time{};

  osc::Sin lfo;
};

template <typename Sample>
ModulatedDelayLine<Sample>::ModulatedDelayLine(double sample_rate,
                                               double max_delay_time,
                                               double center_delay_time,
                                               double lfo_modulation_time,
                                               double lfo_frequency,
                                               double lfo_phase_offset) :
  buffer_size{std::max(0, int(std::floor(sample_rate * max_delay_time)))},
  buffer{std::make_unique<Sample[]>(buffer_size)},
  max_delay_time{max_delay_time},
  center_delay_time{center_delay_time},
  lfo_modulation_time{lfo_modulation_time},
  lfo{sample_rate, lfo_frequency, lfo_phase_offset} {
  //
}

template <typename Sample>
void ModulatedDelayLine<Sample>::change_sample_rate(double to) {
  auto new_buffer_size = std::max(0, int(std::floor(to * max_delay_time)));

  if (new_buffer_size > buffer_size) {
    buffer = std::make_unique<Sample[]>(new_buffer_size);
    buffer_size = new_buffer_size;
    wp = 0;
  }

  lfo.set_sample_rate(to);
}

template <typename Sample>
inline Sample ModulatedDelayLine<Sample>::tick(Sample s, double sr, double feedback) {
  if (wp >= buffer_size) {
    return Sample{};
  }

  const double delay_samples =
    center_delay_time * sr + (lfo.tick() * lfo_modulation_time * sr);
  double rp0 = double(wp) - delay_samples;

  while (rp0 >= buffer_size) {
    rp0 -= buffer_size;
  }

  while (rp0 < 0) {
    rp0 += buffer_size;
  }

  const auto r0 = int(rp0);
  const auto r1 = (r0 + 1) % buffer_size;
  const double frac_rp0 = rp0 - r0;

  const Sample delayed = grove::lerp(frac_rp0, buffer[r0], buffer[r1]);
  const auto res = s + delayed * feedback;

  buffer[wp] = res;
  wp = (wp + 1) % buffer_size;

  return delayed;
}

#ifdef _MSC_VER
#pragma warning( pop )
#endif

}