#pragma once

#include "audio_parameters.hpp"
#include "grove/common/DynamicArray.hpp"
#include <cstdint>
#include <atomic>

namespace grove {

/*
 * AudioEventIDStore
 */

class AudioEventIDStore {
public:
  static uint32_t create() {
    return next_id++;
  }

private:
  static std::atomic<uint32_t> next_id;
};

/*
 * AudioEventData
 */

struct AudioEventData {
  union {
    AudioParameterChange parameter_change;
  };
};

inline AudioEventData make_audio_event_data(const AudioParameterChange& change) {
  AudioEventData data;
  data.parameter_change = change;
  return data;
}

/*
 * AudioEvent
 */

struct AudioEvent {
public:
  enum class Type : uint32_t {
    None = 0,
    NoteOn,
    NoteOff,
    NewDFTFrame,
    NewAudioParameterValue,
    NewRenderBuffer
  };

public:
  Type type;
  uint32_t id;
  union {
    double time;
    uint64_t frame;
  };
  AudioEventData data;
};

using AudioEvents = DynamicArray<AudioEvent, 16>;

/*
 * util
 */

inline AudioEvent make_audio_event(AudioEvent::Type type, double time, const AudioEventData& data) {
  AudioEvent result{};
  result.type = type;
  result.id = AudioEventIDStore::create();
  result.time = time;
  result.data = data;
  return result;
}

inline AudioEvent make_audio_event(AudioEvent::Type type, const AudioEventData& data) {
  return make_audio_event(type, 0.0, data);
}

inline AudioEvent make_new_render_buffer_audio_event() {
  return make_audio_event(AudioEvent::Type::NewRenderBuffer, {});
}

inline AudioEvent
make_monitorable_parameter_audio_event(const AudioParameterIDs& ids,
                                       const AudioParameterValue& param_val,
                                       int write_frame,
                                       int frame_dist) {
  auto change = make_audio_parameter_change(ids, param_val, write_frame, frame_dist);
  auto event_data = make_audio_event_data(change);
  return make_audio_event(AudioEvent::Type::NewAudioParameterValue, event_data);
}

}