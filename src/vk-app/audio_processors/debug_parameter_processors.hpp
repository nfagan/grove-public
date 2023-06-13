#pragma once

#include "grove/audio/audio_parameters.hpp"
#include "grove/audio/AudioEffect.hpp"
#include "grove/audio/oscillator.hpp"

namespace grove {

/*
 * ExampleParameterizedEffect
 */

class ExampleParameterizedEffect : public AudioEffect {
  struct GainLimits {
    static constexpr float min = -10.0f;
    static constexpr float max = 0.0f;

    float minimum() const {
      return min;
    }
    float maximum() const {
      return max;
    }
  };
  struct LfoFreqLimits {
    static constexpr float min = 0.1f;
    static constexpr float max = 5.0f;

    float minimum() const {
      return min;
    }
    float maximum() const {
      return max;
    }
  };

  using WaveformTypeLimits = StaticIntLimits<0, 2>;

public:
  explicit ExampleParameterizedEffect(AudioParameterID node_id);
  ~ExampleParameterizedEffect() override = default;

  void process(Sample* samples,
               AudioEvents* events,
               const AudioParameterChangeView& parameter_changes,
               const AudioRenderInfo& info) override;

  void process_without_parameters(Sample* samples,
                                  AudioEvents* events,
                                  const AudioRenderInfo& info);

  void enable() override;
  void disable() override;
  bool is_enabled() const override;

  AudioParameterDescriptors parameter_descriptors() const override;
  AudioParameterID parameter_parent_id() const override;

private:
  bool parameter_changes_complete() const;

private:
  AudioParameterID node_id;
  AudioParameter<float, GainLimits> gain;
  AudioParameter<float, StaticLimits01<float>> lfo_depth;
  AudioParameter<float, LfoFreqLimits> lfo_freq;
  AudioParameter<int, WaveformTypeLimits> waveform_type;

  osc::WaveTable lfo;
};


}