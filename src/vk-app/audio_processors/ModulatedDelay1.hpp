#pragma once

#include "grove/audio/audio_node.hpp"
#include "grove/audio/delay.hpp"
#include <array>

namespace grove {

struct AudioParameterSystem;

class ModulatedDelay1 : public AudioProcessorNode {
  GROVE_DECLARE_CONSTEXPR_FLOAT_LIMITS(LFOFreqLimits, 0.01f, 10.0f);

public:
  ModulatedDelay1(AudioParameterID node_id, const AudioParameterSystem* sys, bool emit_events);
  ~ModulatedDelay1() override = default;
  GROVE_DECLARE_AUDIO_NODE_INTERFACE()
  GROVE_DECLARE_AUDIO_NODE_PARAMETER_DESCRIPTORS()

private:
  static constexpr int num_channels = 2;

  AudioParameterID node_id;
  const AudioParameterSystem* parameter_system;

  InputAudioPorts input_ports;
  OutputAudioPorts output_ports;

  std::array<audio::ModulatedDelayLine<float>, num_channels> mod_delays;
  std::array<audio::InterpolatedDelayLine<float>, num_channels> rhythmic_delays;

  AudioParameter<float, LFOFreqLimits> lfo_frequency{1.01f};
  AudioParameter<float, StaticLimits01<float>> lfo_representation{};

  bool emit_events;
};

}