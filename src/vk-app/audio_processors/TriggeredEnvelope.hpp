#pragma once

#include "grove/audio/audio_node.hpp"
#include "grove/audio/envelope.hpp"

namespace grove::audio {

class TriggeredEnvelope : public AudioProcessorNode {
public:
  TriggeredEnvelope();
  ~TriggeredEnvelope() override = default;
  GROVE_DECLARE_AUDIO_NODE_INTERFACE()

private:
  InputAudioPorts input_ports;
  OutputAudioPorts output_ports;

  env::ADSRExp<float> envelope{};
  bool triggered{};
};

}