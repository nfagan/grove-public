#include "UIAudioRecordStream.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

#if GROVE_LOGGING_ENABLED
constexpr const char* log_id() {
  return "UIAudioRecordStream";
}
#endif

using UpdateInfo = UIAudioRecordStream::UpdateInfo;
using State = UIAudioRecordStream::State;

std::string make_retrieved_data_message(const AudioRecordStreamResult& result) {
  std::string info_message{"Retrieved data: "};
  info_message += std::to_string(result.size);
  info_message += " bytes.";
  return info_message;
}

void pending_stream_creation(UIAudioRecordStream& stream) {
  auto& future = stream.create_stream_future;
  if (!future->is_ready.load()) {
    return;
  }

  if (future->success) {
    GROVE_LOG_INFO_CAPTURE_META("Created record stream.", log_id());
    stream.record_stream_handle = future->result_handle;
    stream.state = State::ArmRecord;

  } else {
    GROVE_LOG_ERROR_CAPTURE_META("Failed to create record stream.", log_id());
    stream.state = State::Idle;
  }

  future = nullptr;
}

void pending_record_start(UIAudioRecordStream& stream, AudioRecorder* recorder) {
  auto start_recording_command = recorder->start_recording(stream.record_stream_handle);

  if (start_recording_command) {
    GROVE_LOG_INFO_CAPTURE_META("Submitted start record request.", log_id());
    stream.start_stream_future = std::move(start_recording_command);
    stream.state = State::PendingRecordStartConfirmation;
  }
}

void pending_record_start_confirmation(UIAudioRecordStream& stream) {
  auto& future = stream.start_stream_future;
  if (!future->is_ready.load()) {
    return;
  }

  if (future->success) {
    GROVE_LOG_INFO_CAPTURE_META("Started recording.", log_id());
    stream.state = State::Recording;

  } else {
    GROVE_LOG_ERROR_CAPTURE_META("Failed to start recording.", log_id());
    stream.state = State::PendingRecordStop;
  }

  future = nullptr;
}

void pending_record_stop(UIAudioRecordStream& stream, AudioRecorder* recorder) {
  auto stop_recording_command = recorder->stop_recording(stream.record_stream_handle);
  if (stop_recording_command) {
    GROVE_LOG_INFO_CAPTURE_META("Stopping recording.", log_id());
    stream.stop_stream_future = std::move(stop_recording_command);
    stream.state = State::AwaitData;
  }
}

Optional<AudioRecordStreamResult> await_data(UIAudioRecordStream& stream) {
  auto& future = stream.stop_stream_future;
  if (!future->is_ready.load()) {
    return NullOpt{};
  }

  Optional<AudioRecordStreamResult> result;

  if (future->success) {
    auto& stream_result = future->stream_result;

    if (stream_result.success()) {
      auto result_message = make_retrieved_data_message(stream_result);
      GROVE_LOG_INFO_CAPTURE_META(result_message.c_str(), log_id());
      result = std::move(stream_result);

    } else {
      GROVE_LOG_ERROR_CAPTURE_META("Retrieved data, but had recording error.", log_id());
    }
  } else {
    GROVE_LOG_ERROR_CAPTURE_META("Failed to stop recording; no such stream.", log_id());
  }

  stream.state = State::Idle;
  future = nullptr;
  stream.record_stream_handle = {};
  stream.triggered_record_stop = false;

  return result;
}

} //  anon

bool UIAudioRecordStream::create(AudioRecorder* recorder,
                                 AudioRecordChannelSet&& layout, const Transport* transport) {
  layout.finalize();

  if (auto pending_created_stream = recorder->create_stream(std::move(layout), transport)) {
    create_stream_future = std::move(pending_created_stream);
    state = State::PendingStreamCreation;
    return true;

  } else {
    return false;
  }
}

UIAudioRecordStream::UpdateResult
UIAudioRecordStream::update(AudioRecorder* recorder, const UpdateInfo& update_info) {
  UpdateResult result;

  if (state == State::PendingStreamCreation) {
    pending_stream_creation(*this);

  } else if (state == State::ArmRecord) {
    if (update_info.transition(state, record_stream_handle)) {
      state = State::PendingRecordStart;
    }

  } else if (state == State::PendingRecordStart) {
    pending_record_start(*this, recorder);

  } else if (state == State::PendingRecordStartConfirmation) {
    pending_record_start_confirmation(*this);

  } else if (state == State::Recording) {
    if (triggered_record_stop || update_info.transition(state, record_stream_handle)) {
      state = State::PendingRecordStop;
    }

  } else if (state == State::PendingRecordStop) {
    pending_record_stop(*this, recorder);

  } else if (state == State::AwaitData) {
    result.record_result = await_data(*this);
  }

  return result;
}

void UIAudioRecordStream::trigger_record_stop() {
  if (state == State::Recording) {
    triggered_record_stop = true;
  }
}

GROVE_NAMESPACE_END
