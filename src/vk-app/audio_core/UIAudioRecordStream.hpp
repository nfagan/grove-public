#pragma once

#include "grove/audio/AudioRecorder.hpp"
#include <functional>

namespace grove {

class UIAudioRecordStream {
public:
  enum class State {
    Idle,
    PendingStreamCreation,
    ArmRecord,
    PendingRecordStart,
    PendingRecordStartConfirmation,
    Recording,
    PendingRecordStop,
    AwaitData,
  };

  struct UpdateInfo {
    std::function<bool(State, AudioRecordStreamHandle)> transition{
      [](auto&&, auto&&) { return true; }
    };
  };

  struct UpdateResult {
    Optional<AudioRecordStreamResult> record_result;
  };

public:
  bool is_idle() const {
    return state == State::Idle;
  }
  bool is_recording() const {
    return state == State::Recording;
  }

  bool create(AudioRecorder* recorder,
              AudioRecordChannelSet&& layout, const Transport* transport);

  UpdateResult update(AudioRecorder* recorder, const UpdateInfo& update_info);
  void trigger_record_stop();

public:
  State state{State::Idle};

  AudioRecorder::BoxedCreateStreamFuture create_stream_future;
  AudioRecorder::BoxedStartStreamFuture start_stream_future;
  AudioRecorder::BoxedStopStreamFuture stop_stream_future;

  AudioRecordStreamHandle record_stream_handle{};
  bool triggered_record_stop{};
};

}