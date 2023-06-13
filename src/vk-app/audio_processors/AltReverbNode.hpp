#pragma once

#include "grove/audio/audio_node.hpp"
#include "grove/audio/envelope.hpp"
#include "Reverb1.hpp"

namespace grove {

struct AudioParameterSystem;

class AltReverbNode : public AudioProcessorNode {
  static constexpr float default_feedback = 0.5f;

public:
  AltReverbNode(AudioParameterID node_id, const AudioParameterSystem* parameter_system);
  ~AltReverbNode() override = default;
  GROVE_DECLARE_AUDIO_NODE_INTERFACE()
  GROVE_DECLARE_AUDIO_NODE_PARAMETER_DESCRIPTORS()
  uint32_t get_id() const override {
    return node_id;
  }

private:
  AudioParameterID node_id;

  const AudioParameterSystem* parameter_system;

  AudioParameter<float, StaticLimits01<float>> mix{0.0f};
  AudioParameter<float, StaticLimits01<float>> fdn_feedback{default_feedback};
  AudioParameter<float, StaticLimits01<float>> fixed_osc_mix{0.0f};
  AudioParameter<float, StaticLimits01<float>> signal_representation{0.0f};

  Reverb1 reverb;
  double fixed_osc_sin_phase{};
  double fixed_osc_sin_freq{};
  env::ADSRExp<float> fixed_osc_env;
  double last_sample_rate{default_sample_rate()};
};

}