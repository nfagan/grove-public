#pragma once

#include "grove/audio/audio_node.hpp"

namespace grove {

class DebugTuning : public AudioProcessorNode {
public:
  struct Params {
    static constexpr int num_params = 1;
    AudioParameter<float, StaticLimits01<float>> scale_frac{0.0f};
  };

public:
  DebugTuning(uint32_t node_id);
  ~DebugTuning() override = default;

  GROVE_DECLARE_AUDIO_NODE_INTERFACE()
  GROVE_DECLARE_AUDIO_NODE_PARAMETER_DESCRIPTORS()
  uint32_t get_id() const override {
    return node_id;
  }

private:
  uint32_t node_id;
  uint8_t note_number{};
  double osc_phase{};

  Params params;
};

}