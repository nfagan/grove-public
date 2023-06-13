#pragma once

#include "grove/audio/audio_node.hpp"
#include "grove/audio/delay.hpp"
#include "grove/audio/oscillator.hpp"

namespace grove {

struct AudioParameterSystem;

class RhythmicDelay1 : public AudioProcessorNode {
public:
  GROVE_DECLARE_CONSTEXPR_FLOAT_LIMITS(DelayTimeLimits, 0.001f, 0.3f);
  static constexpr float default_delay_time = 0.25f;

public:
  RhythmicDelay1(AudioParameterID node_id, const AudioParameterSystem* parameter_system);
  ~RhythmicDelay1() override = default;

  GROVE_DECLARE_AUDIO_NODE_INTERFACE()
  GROVE_DECLARE_AUDIO_NODE_PARAMETER_DESCRIPTORS()
  uint32_t get_id() const override {
    return node_id;
  }

private:
  AudioParameterID node_id;

  InputAudioPorts input_ports;
  OutputAudioPorts output_ports;

  const AudioParameterSystem* parameter_system;

  AudioParameter<float, DelayTimeLimits> delay_time{default_delay_time};
  AudioParameter<float, StaticLimits01<float>> mix{0.5f};
  AudioParameter<float, StaticLimits01<float>> chorus_mix{0.5f};
  AudioParameter<float, StaticLimits01<float>> noise_mix{0.0f};
  AudioParameter<float, StaticLimits01<float>> signal_representation{0.0f};

  double sample_rate;
  audio::InterpolatedDelayLine<Sample2> delay;
  std::array<audio::ModulatedDelayLine<float>, 2> mod_delays;
  osc::WaveTable noise_osc;
  osc::Sin noise_amp_lfo;
  float noise_gain{0.25f};
};

}