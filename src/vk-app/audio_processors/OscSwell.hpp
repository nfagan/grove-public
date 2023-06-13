#pragma once

#include "grove/audio/audio_node.hpp"
#include "grove/audio/envelope.hpp"
#include "grove/audio/oscillator.hpp"

namespace grove {

class AudioScale;

class OscSwell : public AudioProcessorNode {
public:
  static constexpr int num_voices = 4;

public:
  OscSwell(AudioParameterID node_id, const AudioScale* scale, bool enable_events);
  ~OscSwell() override = default;
  GROVE_DECLARE_AUDIO_NODE_INTERFACE()
  GROVE_DECLARE_AUDIO_NODE_PARAMETER_DESCRIPTORS()

private:
  AudioParameterID node_id;

  InputAudioPorts input_ports;
  OutputAudioPorts output_ports;

  const AudioScale* scale;

  env::ADSRExp<float> envelopes[4];
  osc::Sin oscillators[4];
  uint8_t active_notes[4]{};
  uint8_t pending_notes[4]{};
  uint8_t num_pending_notes{};

  audio::ExpInterpolated<float> input_gain{1.0f};

  AudioParameter<float, StaticLimits01<float>> signal_repr{0.0f};
  bool events_enabled;
};

}