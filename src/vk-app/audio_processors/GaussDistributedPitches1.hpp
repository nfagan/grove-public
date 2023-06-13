#pragma once

#include "grove/audio/audio_node.hpp"

namespace grove {

class AudioScale;
struct AudioParameterSystem;

class GaussDistributedPitches1 : public AudioProcessorNode {
public:
  static constexpr int num_voices = 8;
  static constexpr int num_lobes = 4;
  static constexpr float min_sigma = 0.125f * 0.5f;
  static constexpr float max_sigma = 2.0f;
  static constexpr int min_mu = -36;
  static constexpr int max_mu = 36;

  struct Voice {
    bool on;
    bool st_has_kb_offset;
    double duration;
    float st;
    double phase;
  };

  struct Params {
    static constexpr int num_params = num_lobes * 3 + 1;

    Params();
    AudioParameter<int, StaticIntLimits<min_mu, max_mu>> mus[num_lobes];
    AudioParameter<float, StaticLimits01<float>> sigmas[num_lobes];
    AudioParameter<float, StaticLimits01<float>> scales[num_lobes];
    AudioParameter<int, StaticIntLimits<0, 1>> follow_keyboard{1};
  };

public:
  GaussDistributedPitches1(
    uint32_t node_id, const AudioScale* scale, const AudioParameterSystem* param_sys);
  ~GaussDistributedPitches1() override = default;
  GROVE_DECLARE_AUDIO_NODE_INTERFACE()
  GROVE_DECLARE_AUDIO_NODE_PARAMETER_DESCRIPTORS()
  uint32_t get_id() const override {
    return node_id;
  }

private:
  void update_distribution();

private:
  uint32_t node_id;

  const AudioScale* scale;
  const AudioParameterSystem* param_sys;

  float kb_semitone{};
  Voice voices[num_voices]{};
  Params params;
};

}