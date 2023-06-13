#pragma once

#include "grove/audio/audio_node.hpp"
#include "grove/audio/oscillator.hpp"
#include "grove/audio/envelope.hpp"

namespace grove {

struct AudioParameterSystem;

class RandomizedInstrumentNode : public AudioProcessorNode {
public:
  RandomizedInstrumentNode(AudioParameterID node_id, const AudioParameterSystem* parameter_system);
  ~RandomizedInstrumentNode() override = default;

  GROVE_DECLARE_AUDIO_NODE_INTERFACE()
  GROVE_DECLARE_AUDIO_NODE_PARAMETER_DESCRIPTORS()

private:
  void randomize_frequency();
  void apply_new_waveform();

private:
  AudioParameterID node_id;

  const AudioParameterSystem* parameter_system;

  InputAudioPorts input_ports;
  OutputAudioPorts output_ports;

  osc::WaveTable oscillator{};
  env::ADSRExp<float> envelope{};

  int key{0};
  int note_number{0};

  AudioParameter<int, StaticIntLimits<0, 2>> waveform_type{0};
  AudioParameter<int, StaticIntLimits<0, 1>> scale_type{0};
  AudioParameter<float, StaticLimits01<float>> signal_representation{0.0f};
  AudioParameter<int, StaticIntLimits<0, 127>> note_number_representation{0};
};

}