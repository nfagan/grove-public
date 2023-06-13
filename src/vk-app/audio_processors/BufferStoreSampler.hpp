#pragma once

#include "grove/audio/audio_node.hpp"
#include "grove/audio/audio_buffer.hpp"
#include "grove/audio/envelope.hpp"
#include "grove/audio/voice_allocation.hpp"

namespace grove {

class AudioBufferStore;
class AudioScale;

class BufferStoreSampler : public AudioProcessorNode {
  static constexpr int num_voices = 4;
public:
  BufferStoreSampler(uint32_t node_id, const AudioBufferStore* buffer_store,
                     AudioBufferHandle buffer_handle, const AudioScale* scale,
                     bool enable_events = false);
  ~BufferStoreSampler() override = default;
  GROVE_DECLARE_AUDIO_NODE_INTERFACE()
  GROVE_DECLARE_AUDIO_NODE_PARAMETER_DESCRIPTORS()
  uint32_t get_id() const override {
    return node_id;
  }

private:
  uint32_t node_id;
  const AudioBufferStore* buffer_store;
  AudioBufferHandle buffer_handle;
  const AudioScale* scale;
  bool enable_events;

  std::array<double, num_voices> frame_indices{};
  std::array<double, num_voices> rate_multipliers{};
  std::array<uint8_t, num_voices> note_numbers{};
  std::array<env::ADSRExp<float>, num_voices> envelopes;
  audio::VoiceAllocator<num_voices> voice_allocator;

  AudioParameter<float, StaticLimits01<float>> signal_repr{0.0f};

  InputAudioPorts input_ports;
  OutputAudioPorts output_ports;
};

}