#pragma once

#include "grove/audio/audio_node.hpp"

namespace grove {

struct AudioParameterSystem;

}

namespace grove::audio {

class TriggerNode : public AudioProcessorNode {
public:
  TriggerNode(AudioParameterID node_id, const AudioParameterSystem* parameter_system,
              float low = 0.0f, float high = 1.0f, int pulse_duration_samples = 50);
  ~TriggerNode() override = default;
  GROVE_DECLARE_AUDIO_NODE_INTERFACE()
  GROVE_DECLARE_AUDIO_NODE_PARAMETER_DESCRIPTORS()

private:
  AudioParameterID node_id;

  InputAudioPorts input_ports;
  OutputAudioPorts output_ports;

  const AudioParameterSystem* parameter_system;
  AudioParameter<int, StaticLimits01<int>> trigger{0};

  float low{};
  float high{};
  int pulse_duration_samples{};
  int pulse_index{};
};

}