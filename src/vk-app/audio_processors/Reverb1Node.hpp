#pragma once

#include "Reverb1.hpp"
#include "grove/audio/audio_node.hpp"
#include "grove/audio/audio_parameters.hpp"

namespace grove {

struct AudioParameterSystem;

class Reverb1Node : public AudioProcessorNode {
public:
  enum class Layout {
    Sample2,
    TwoChannelFloat,
  };
  struct Params {
    float default_mix{0.5f};
    float default_fdn_feedback{0.98f};
  };

public:
  Reverb1Node(AudioParameterID node_id, int param_offset,
              const AudioParameterSystem* param_sys, Layout input_layout, const Params& params);
  ~Reverb1Node() override = default;

  GROVE_DECLARE_AUDIO_NODE_INTERFACE()
  GROVE_DECLARE_AUDIO_NODE_PARAMETER_DESCRIPTORS()

  void render_set_mix(float value) {
    mix.set(value);
  }

  void render_set_feedback_from_fraction(float value) {
    fdn_feedback.set_from_fraction(value);
  }

public:
  AudioParameterID node_id;
  int parameter_offset;

  const AudioParameterSystem* parameter_system;

  Layout layout;
  InputAudioPorts input_ports;
  OutputAudioPorts output_ports;

  double last_sample_rate{default_sample_rate()};

  Reverb1 reverb;

  Params params{};
  AudioParameter<float, StaticLimits01<float>> mix;
  AudioParameter<float, Reverb1::FDNFeedbackLimits> fdn_feedback;
};

}