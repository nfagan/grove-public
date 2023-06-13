#pragma once

#include "grove/audio/audio_node.hpp"
#include "grove/audio/audio_buffer.hpp"

namespace grove {

class AudioBufferStore;
class AudioScale;
struct AudioParameterSystem;
class Transport;

class ChimeSampler : public AudioProcessorNode {
public:
  enum class NoteState : uint8_t {
    Idle = 0,
    AwaitingOnset,
    On,
    Elapsed,
    Decaying
  };

  struct Voice {
    NoteState state;
    NoteState timeout_state;
    float time_left;
    float timeout;
    float decay_time;
    double frame_index;
    double semitone;
    uint8_t buff_index;
    float gain;
    audio::Quantization next_quantization;
    bool quantized;
  };

  struct Params {
    static constexpr int num_params = 5;
    AudioParameter<float, StaticLimits01<float>> p_quantized{0.0f};
    AudioParameter<float, StaticLimits01<float>> chime_mix{1.0f};
    AudioParameter<int, StaticIntLimits<0, 3>> duration_index{0};
    AudioParameter<int, StaticIntLimits<0, 1>> buffer_set_index{0};
    AudioParameter<int, StaticIntLimits<0, 1>> note_set_index{0};
  };

  static constexpr int num_voices = 8;

public:
  ChimeSampler(
    uint32_t node_id, const AudioBufferStore* buff_store, const AudioScale* scale,
    const Transport* transport, const AudioParameterSystem* param_sys,
    uint32_t pitch_sample_group, AudioBufferHandle bg_buff,
    const AudioBufferHandle* note_buffs, int num_note_buffs);
  ~ChimeSampler() override = default;
  uint32_t get_id() const override {
    return node_id;
  }
  GROVE_DECLARE_AUDIO_NODE_INTERFACE()
  GROVE_DECLARE_AUDIO_NODE_PARAMETER_DESCRIPTORS()

private:
  uint32_t node_id;

  const AudioBufferStore* buffer_store;
  const AudioScale* scale;
  const Transport* transport;
  const AudioParameterSystem* param_sys;
  uint32_t pitch_sample_group;

  AudioBufferHandle bg_buff_handle;
  AudioBufferHandle note_buff_handles[4];
  int num_note_buff_handles{};

  double bg_frame_index{};

  Voice voices[num_voices]{};
  double time_left_before_new_event{};

  double kb_semitone{semitone_a4()};
  double global_semitone_offsets[16]{};
  int num_global_semitone_offsets{};

  Params params{};
};

}