#include "UtilityEffect.hpp"
#include "grove/common/common.hpp"
#include "grove/math/util.hpp"
#include "grove/math/constants.hpp"
#include <cmath>

GROVE_NAMESPACE_BEGIN

UtilityEffect::UtilityEffect() :
  pan_position(0.5),
  gain(amplitude_to_db(1.0)),
  enabled(true) {
  //
}

void UtilityEffect::adjust_pan_position(double incr) {
  set_pan_position(pan_position.load() + incr);
}

void UtilityEffect::set_pan_position(double to) {
  pan_position.store(clamp(to, 0.0, 1.0));
}

void UtilityEffect::adjust_gain(double incr) {
  set_gain(gain.load() + incr);
}

void UtilityEffect::set_gain(double gn) {
  gain.store(gn);
}

void UtilityEffect::mute() {
  set_gain(-infinity());
}

void UtilityEffect::disable() {
  enabled.store(false);
}

void UtilityEffect::enable() {
  enabled.store(true);
}

bool UtilityEffect::is_enabled() const {
  return enabled.load();
}

void UtilityEffect::process(Sample* samples,
                            AudioEvents*,
                            const AudioParameterChangeView&,
                            const AudioRenderInfo& info) {
  if (!enabled.load()) {
    return;
  }

  double load_pan_pos = pan_position.load();
  double load_gain = gain.load();
  double amp = db_to_amplitude(load_gain);

  const auto theta = pi_over_two() * load_pan_pos - pi_over_four();
  const double ct = std::cos(theta);
  const double st = std::sin(theta);

  const auto atten_factor = std::sqrt(2.0) * 0.5;
  const double a_left = atten_factor * (ct - st);
  const double a_right = atten_factor * (ct + st);

  for (int i = 0; i < info.num_frames; i++) {
    for (int j = 0; j < info.num_channels; j++) {
      const int ind = i * info.num_channels + j;
      double pan_factor = j == 0 ? a_left : j == 1 ? a_right : 1.0;
      (void) pan_factor;

      auto sample = samples[ind];
      sample *= Sample(amp);
//      sample *= pan_factor;
      samples[ind] = sample;
    }
  }
}

GROVE_NAMESPACE_END
