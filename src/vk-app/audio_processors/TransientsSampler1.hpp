#pragma once

#include "grove/audio/audio_node.hpp"
#include "grove/audio/envelope.hpp"
#include "grove/audio/delay.hpp"
#include "grove/audio/AudioBufferStore.hpp"
#include "grove/common/Optional.hpp"

namespace grove {

class Transport;

class TransientsSampler1 : public AudioProcessorNode {
public:
  struct Params {
    static constexpr int num_params = 5;
    AudioParameter<float, StaticLimits01<float>> p_local_quantized{0.97f};
    AudioParameter<float, StaticLimits01<float>> p_durations_fan_out{0.005f};
    AudioParameter<float, StaticLimits01<float>> p_global_timeout{0.005f};
    AudioParameter<int, StaticIntLimits<0, 2>> local_quantization{1};
    AudioParameter<float, StaticLimits01<float>> local_time{0.0f};
  };

public:
  TransientsSampler1(
    uint32_t node_id, const Transport* transport, const AudioBufferStore* buff_store,
    AudioBufferHandle buff_handle, const uint32_t* transient_onsets, int num_onsets);
  ~TransientsSampler1() override = default;
  GROVE_DECLARE_AUDIO_NODE_INTERFACE()
  GROVE_DECLARE_AUDIO_NODE_PARAMETER_DESCRIPTORS()
  uint32_t get_id() const override {
    return node_id;
  }

private:
  uint32_t node_id;

  const Transport* transport;
  const AudioBufferStore* buff_store;
  AudioBufferHandle buff_handle;

  uint32_t onsets[32]{};
  int num_onsets{};

  double buff_fi{};
  double time_left{};
  bool local_elapsed{true};
  double local_elapsed_time{};
  double global_timeout_elapsed_time{};
  double global_timeout_time{};
  double fan_out_timeout{};
  double inter_timeout_time{};
  bool right_on{};

  Optional<audio::Quantization> local_quant;
  uint8_t local_duration_state{};
  uint8_t local_duration_index{};
  uint8_t global_timeout_state{};

  Params params;
};

}