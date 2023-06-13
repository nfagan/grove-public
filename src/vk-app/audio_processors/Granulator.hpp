#pragma once

#include "grove/audio/oscillator.hpp"
#include "grove/audio/audio_buffer.hpp"
#include "grove/math/random.hpp"

namespace grove {

class Granulator {
public:
  struct Params {
    double rate_multiplier{1.0};
    double lfo_depth{0.0};
    double lfo_frequency{8.0};
    double center_granule_period{0.3};
  };

public:
  Sample2 tick_sample2(const unsigned char* data,
                       const AudioBufferDescriptor& descriptor,
                       double output_sample_rate,
                       const Params& params);

  double get_frame_index() const {
    return frame_index;
  }

private:
  double tick_granule_period(double output_sample_rate, const Params& params);
  void tick_frame_indices(double src_sr,
                          double output_sr,
                          double rate_multiplier,
                          int num_source_frames,
                          int frames_per_granule,
                          int num_granules);

  static double evaluate_gauss_win(double frac_n, double size);
  static double amplitude_window(double ind, int frames_per_granule);

private:
  osc::Sin lfo;

  double frame_index{};
  double granule_index{};
};

inline double Granulator::evaluate_gauss_win(double frac_n, double size) {
  const double a = 2.5;
  double n = frac_n * size - size * 0.5;
  return std::exp(-0.5 * std::pow(a * n / (size / 2.0), 2.0));
}

inline double Granulator::amplitude_window(double ind, int frames_per_granule) {
  const double frac_granule = ind / double(frames_per_granule);
  return evaluate_gauss_win(frac_granule, frames_per_granule);
}

inline void Granulator::tick_frame_indices(double src_sr,
                                           double output_sr,
                                           double rate_multiplier,
                                           int num_source_frames,
                                           int frames_per_granule,
                                           int num_granules) {
  const auto incr = frame_index_increment(src_sr, output_sr, rate_multiplier);

  frame_index += incr;
  granule_index += incr;

  while (frame_index >= num_source_frames) {
    frame_index -= num_source_frames;
  }

  if (granule_index >= frames_per_granule) {
    granule_index = 0.0;
    const auto new_granule = int(grove::urand() * float(num_granules));
    frame_index = new_granule * frames_per_granule;
  }
}

inline double Granulator::tick_granule_period(double output_sample_rate, const Params& params) {
  lfo.set_frequency(params.lfo_frequency);
  lfo.set_sample_rate(output_sample_rate);

  const auto lfo_amount = lfo.tick() * (params.lfo_depth * 0.5) * params.center_granule_period;
  return params.center_granule_period + lfo_amount;
}

inline Sample2 Granulator::tick_sample2(const unsigned char* data,
                                        const AudioBufferDescriptor& descriptor,
                                        double output_sample_rate,
                                        const Params& params) {
  assert(descriptor.num_channels() == 2);

  const auto start_frame = int(frame_index);
  const auto num_source_frames = int(descriptor.total_num_frames());

  if (start_frame >= num_source_frames) {
    return Sample2{};
  }

  auto granule_period = tick_granule_period(output_sample_rate, params);

  auto frames_per_granule =
    std::max(1, std::min(int(granule_period * descriptor.sample_rate), num_source_frames));
  auto num_granules = num_source_frames / frames_per_granule;

  auto interp_info = util::make_linear_interpolation_info(frame_index, num_source_frames);
  const double gauss_win = amplitude_window(granule_index, frames_per_granule);

  Sample2 result{};
  for (int i = 0; i < 2; i++) {
    auto descr = descriptor.layout.channel_descriptor(i);
    assert(descr.is_float());

    if (interp_info.i0 < uint64_t(num_source_frames) &&
        interp_info.i1 < uint64_t(num_source_frames)) {
      auto sample = Sample(util::tick_interpolated_float(data, descr, interp_info));
      result.samples[i] = Sample(sample * gauss_win);
    }
  }

  tick_frame_indices(descriptor.sample_rate, output_sample_rate,
                     params.rate_multiplier, num_source_frames, frames_per_granule, num_granules);

  return result;
}

}