#pragma once

#include "grove/audio/audio_node.hpp"
#include "grove/audio/delay.hpp"
#include <array>

namespace grove {

class Transport;

class Bender : public AudioProcessorNode {
public:
  GROVE_DECLARE_CONSTEXPR_FLOAT_LIMITS(DelayTimeMsLimits, 5.0f, 50.0f);

public:
  Bender(AudioParameterID node_id, const Transport* transport, bool emit_events);
  ~Bender() override = default;
  GROVE_DECLARE_AUDIO_NODE_INTERFACE()
  GROVE_DECLARE_AUDIO_NODE_PARAMETER_DESCRIPTORS()
  uint32_t get_id() const override {
    return node_id;
  }

private:
  AudioParameterID node_id;

  InputAudioPorts input_ports;
  OutputAudioPorts output_ports;

  AudioParameter<int, StaticLimits01<int>> quantization_representation{0};
  AudioParameter<float, StaticLimits01<float>> signal_representation{0.0f};

  const Transport* transport;
  audio::ModulatedDelayLine<Sample2> short_delay;
  AudioParameter<float, DelayTimeMsLimits> delay_time{DelayTimeMsLimits::min};

  ScoreCursor cursor{};
  double last_quantum{-1.0};
  bool target_short{};
  bool high_epoch{};
  bool emit_events{};
  audio::Quantization quantization{audio::Quantization::Eighth};
};

}