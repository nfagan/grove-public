#pragma once

#include "../audio_node.hpp"
#include "../envelope.hpp"
#include "../oscillator.hpp"

namespace grove {

struct AudioParameterSystem;

class RandomizedEnvelopeNode : public AudioProcessorNode {
public:
  explicit RandomizedEnvelopeNode(AudioParameterID node_id,
                                  const AudioParameterSystem* parameter_system,
                                  int num_outputs = 1,
                                  bool emit_events = false);
  ~RandomizedEnvelopeNode() override = default;
  GROVE_DECLARE_AUDIO_NODE_INTERFACE()
  GROVE_DECLARE_AUDIO_NODE_PARAMETER_DESCRIPTORS()
  uint32_t get_id() const override {
    return node_id;
  }

private:
  AudioParameterID node_id;
  OutputAudioPorts output_ports;

  const AudioParameterSystem* parameter_system;
  env::ADSRExp<float> envelope{};
  osc::Sin lfo;

  AudioParameter<float, StaticLimits01<float>> amplitude_modulation_amount{0.0f};
  AudioParameter<float, StaticLimits01<float>> envelope_representation{0.0f};

  bool emit_events{};
};

}