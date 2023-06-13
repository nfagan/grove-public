#pragma once

#include "grove/audio/filter.hpp"
#include "grove/audio/delay.hpp"
#include "grove/audio/audio_parameters.hpp"
#include "grove/math/Mat4.hpp"

namespace grove {

namespace impl {

using FDNDelays = DynamicArray<audio::SimpleDelayLine<float>, 4>;
using FDNFilters = DynamicArray<audio::LinearFilter<double, 3, 3>, 4>;

constexpr Mat4<double> hadamard4() {
  return Mat4<double>{
    0.5, 0.5, 0.5, 0.5,
    0.5, -0.5, 0.5, -0.5,
    0.5, 0.5, -0.5, -0.5,
    0.5, -0.5, -0.5, 0.5
  };
}

inline Sample fdn_tick(Sample u, FDNDelays& delays, FDNFilters& filters,
                       const Mat4<double>& A, double feedback) {
  const auto n =
    std::min(4, std::min(int(delays.size()), int(filters.size())));

  Vec4<double> v0{};
  for (int j = 0; j < n; j++) {
    v0[j] = filters[j].tick(delays[j].current());
  }

  auto mixed = A * v0;
  double curr0{};

  for (int j = 0; j < n; j++) {
    auto v = mixed[j] * feedback + u;
    delays[j].tick(Sample(v));
    curr0 += v;
  }

  if (n > 0) {
    curr0 /= double(n);
  }

  return Sample(curr0);
}

} //  impl

struct Reverb1 {
public:
  GROVE_DECLARE_CONSTEXPR_FLOAT_LIMITS(FDNFeedbackLimits, 0.9f, 0.98f);

public:
  Reverb1();
  void set_sample_rate(double to);
  Sample2 tick(Sample2 src, double sample_rate, float feedback, float mix);

private:
  audio::LinearFilter<double, 11, 11> lp0;
  audio::LinearFilter<double, 11, 11> lp1;

  audio::SimpleDelayLine<float> initial_delay0{4409};
  audio::SimpleDelayLine<float> initial_delay1{5717};
  float initial_delay_feedback{0.75f};

  audio::ModulatedDelayLine<float> chorus0{default_sample_rate(), 0.1, 0.003, 0.0019, 1.01};
  audio::ModulatedDelayLine<float> chorus1{default_sample_rate(), 0.1, 0.007, 0.0019, 1.01};

  impl::FDNFilters fdn_filters0;
  impl::FDNFilters fdn_filters1;

  impl::FDNDelays fdn_delays0;
  impl::FDNDelays fdn_delays1;
};

inline void Reverb1::set_sample_rate(double to) {
  chorus0.change_sample_rate(to);
  chorus1.change_sample_rate(to);
}

inline Sample2 Reverb1::tick(Sample2 src, double sample_rate, float feedback, float mix) {
  constexpr auto A = impl::hadamard4();

  auto dest = src;

  //  Band-pass
  dest.samples[0] = Sample(lp0.tick(dest.samples[0]));
  dest.samples[1] = Sample(lp1.tick(dest.samples[1]));

  //  Initial delay.
  auto curr0 = initial_delay0.current();
  curr0 = chorus0.tick(curr0, sample_rate, 0.0);
  initial_delay0.tick(dest.samples[0] + curr0 * initial_delay_feedback);

  auto curr1 = initial_delay1.current();
  curr1 = chorus1.tick(curr1, sample_rate, 0.0);
  initial_delay1.tick(dest.samples[1] + curr1 * initial_delay_feedback);

  dest.samples[0] = curr0;
  dest.samples[1] = curr1;

  //  FDN.
  dest.samples[0] = impl::fdn_tick(dest.samples[0], fdn_delays0, fdn_filters0, A, feedback);
  dest.samples[1] = impl::fdn_tick(dest.samples[1], fdn_delays1, fdn_filters1, A, feedback);

  //  Mix.
  return lerp(mix, src, dest);
}

}