#pragma once

#include "../AudioEffect.hpp"
#include <atomic>

namespace grove {

class UtilityEffect : public AudioEffect {
public:
  UtilityEffect();
  ~UtilityEffect() override = default;

  void process(Sample* samples,
               AudioEvents* events,
               const AudioParameterChangeView&,
               const AudioRenderInfo& info) override;

  void enable() override;
  void disable() override;
  bool is_enabled() const override;

  void adjust_pan_position(double incr);
  void set_pan_position(double to);

  void adjust_gain(double incr);
  void set_gain(double gain);
  void mute();

private:
  std::atomic<double> pan_position;
  std::atomic<double> gain;
  std::atomic<bool> enabled;
};

}