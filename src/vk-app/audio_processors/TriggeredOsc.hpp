#pragma once

#include "grove/audio/audio_node.hpp"
#include "grove/audio/oscillator.hpp"
#include "grove/audio/envelope.hpp"

namespace grove {

class AudioScale;
struct AudioParameterSystem;

class TriggeredOsc : public AudioProcessorNode {
public:
  GROVE_DECLARE_CONSTEXPR_FLOAT_LIMITS(AmpModFreq, 0.5f, 10.0f);

  struct Params {
    AudioParameter<float, StaticLimits01<float>> amp_mod_depth{0.0f};
    AudioParameter<float, AmpModFreq> amp_mod_freq{0.5f};
    AudioParameter<int, StaticIntLimits<-12, 12>> semitone_offset{0};
    AudioParameter<int, StaticIntLimits<0, 255>> monitor_note_number{0};
    AudioParameter<float, StaticLimits01<float>> signal_representation{0.0f};
  };

public:
  TriggeredOsc(AudioParameterID node_id, const AudioScale* scale,
               const AudioParameterSystem* param_sys);
  ~TriggeredOsc() override = default;

  GROVE_DECLARE_AUDIO_NODE_INTERFACE()
  GROVE_DECLARE_AUDIO_NODE_PARAMETER_DESCRIPTORS()
  uint32_t get_id() const override {
    return node_id;
  }

private:
  InputAudioPorts input_ports;
  OutputAudioPorts output_ports;

  AudioParameterID node_id;
  const AudioScale* scale;
  const AudioParameterSystem* param_sys;

  uint8_t current_note_number{midi_note_number_a4()};
  osc::Sin amp_mod;

  Params params;
  osc::Sin osc;
  env::ADSRExp<float> env;
};

}