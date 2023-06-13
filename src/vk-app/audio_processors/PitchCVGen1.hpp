#pragma once

#include "grove/audio/audio_node.hpp"
#include "grove/audio/envelope.hpp"
#include "grove/audio/oscillator.hpp"

namespace grove {

class Transport;
struct AudioParameterSystem;

class PitchCVGen1 : public AudioProcessorNode {
public:
  PitchCVGen1(AudioParameterID node_id, const Transport* transport,
              const AudioParameterSystem* parameter_system);
  ~PitchCVGen1() override = default;
  GROVE_DECLARE_AUDIO_NODE_INTERFACE()
  GROVE_DECLARE_AUDIO_NODE_PARAMETER_DESCRIPTORS()

private:
  AudioParameterID node_id;

  InputAudioPorts input_ports;
  OutputAudioPorts output_ports;

  const Transport* transport;
  const AudioParameterSystem* parameter_system;

  PitchClass center_pitch_class{};
  int8_t center_pitch_octave{3};
  double last_quantum{-1.0};

  audio::ExpInterpolated<float> pitch_cv;
  osc::Sin pitch_cv_lfo;
  AudioParameter<float, StaticLimits01<float>> pitch_cv_mod_depth{0.0f};
};

}