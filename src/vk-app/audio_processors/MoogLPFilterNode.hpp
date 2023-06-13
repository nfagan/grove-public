#pragma once

#include "filters.hpp"
#include "grove/audio/audio_node.hpp"
#include <array>

namespace grove {

struct AudioParameterSystem;

class MoogLPFilterNode : public AudioProcessorNode {
  GROVE_DECLARE_CONSTEXPR_FLOAT_LIMITS(CutoffLimits, 50.0f, 5e3);
  GROVE_DECLARE_CONSTEXPR_FLOAT_LIMITS(ResonanceLimits, 0.0f, 0.9f);

  static constexpr auto cutoff_default = CutoffLimits::max;
  static constexpr auto resonance_default = ResonanceLimits::min;

public:
  MoogLPFilterNode(AudioParameterID node_id, const AudioParameterSystem* parameter_system);
  ~MoogLPFilterNode() override = default;

  GROVE_DECLARE_AUDIO_NODE_INTERFACE()
  GROVE_DECLARE_AUDIO_NODE_PARAMETER_DESCRIPTORS()

  uint32_t get_id() const override {
    return node_id;
  }

private:
  AudioParameterID node_id;

  const AudioParameterSystem* parameter_system;
  AudioParameter<float, CutoffLimits> cutoff{cutoff_default};
  AudioParameter<float, ResonanceLimits> resonance{resonance_default};

  std::array<MoogLPFilterState, 2> state{};
};

}