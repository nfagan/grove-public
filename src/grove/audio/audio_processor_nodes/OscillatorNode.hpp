#pragma once

#include "../audio_node.hpp"
#include "../oscillator.hpp"

namespace grove {

struct AudioParameterSystem;
class Transport;

class OscillatorNode : public AudioProcessorNode {
public:
//  GROVE_DECLARE_CONSTEXPR_FLOAT_LIMITS(FreqLimits, 0.1f, 10.0f);

  struct Params {
    AudioParameter<int, StaticIntLimits<0, 2>> waveform{0};
    AudioParameter<int, StaticIntLimits<0, 1>> tempo_sync{1};
    AudioParameter<float, StaticLimits01<float>> frequency{0.0f};
  };

public:
  OscillatorNode(uint32_t node_id, const AudioParameterSystem* param_sys,
                 const Transport* transport, int num_channels);
  ~OscillatorNode() override = default;
  GROVE_DECLARE_AUDIO_NODE_INTERFACE()
  GROVE_DECLARE_AUDIO_NODE_PARAMETER_DESCRIPTORS()

private:
  uint32_t node_id;
  osc::WaveTable oscillator;
  ScoreCursor cursor{};
  const AudioParameterSystem* parameter_system;
  const Transport* transport;
  int num_channels;
  Params params;
};

}