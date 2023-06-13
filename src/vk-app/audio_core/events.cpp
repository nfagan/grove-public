#include "events.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include "grove/audio/AudioCore.hpp"
#include "grove/audio/AudioEventSystem.hpp"
#include "grove/common/vector_util.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

[[maybe_unused]] constexpr const char* logging_id() {
  return "audio_core/events";
}

[[maybe_unused]] void update_audio_event_system(audio::EventUpdateContext& context,
                                                const audio::EventUpdateInfo& info,
                                                audio::EventUpdateResult& result) {
  auto curr_time = info.is_stream_started ?
    Optional<double>(info.audio_core.audio_stream.current_time()) : NullOpt{};

  auto event_update_res = audio_event_system::ui_update(curr_time);
  for (auto& evt : event_update_res.newly_acquired) {
    if (evt.type == AudioEvent::Type::NewAudioParameterValue) {
      context.ui_parameter_change_list.parameter_change_events.push_back(evt);
    }
  }

  for (auto& evt : event_update_res.newly_ready) {
    if (evt.type == AudioEvent::Type::NewRenderBuffer) {
      context.new_render_buffer_event_ids.push_back(evt.id);
    }
  }

  if (audio_event_system::ui_check_dropped_events()) {
    result.any_event_system_dropped_events = true;
    GROVE_LOG_SEVERE_CAPTURE_META("Dropped some AudioEventSystem events.", logging_id());
  }
  if (audio_event_system::ui_check_render_buffer_overflow()) {
    result.any_event_system_dropped_events = true;
    GROVE_LOG_SEVERE_CAPTURE_META("AudioEventSystem render buffer overflow.", logging_id());
  }
}

void clear_context(audio::EventUpdateContext& context) {
  context.ui_parameter_change_list.clear();
  context.new_render_buffer_event_ids.clear();
}

void update_audio_events(audio::EventUpdateContext& context,
                         const audio::EventUpdateInfo& update_info) {
  auto& spectrum_analyzer = update_info.spectrum_analyzer;
  auto& audio_stream = update_info.audio_core.audio_stream;
  auto& pending_analysis_frames = context.pending_analysis_frames;
  auto& pending_audio_events = context.pending_audio_events;

  std::vector<int> to_remove;

  for (int i = 0; i < int(pending_audio_events.size()); i++) {
    auto curr_time = update_info.is_stream_started ? audio_stream.current_time() : -1.0;
    const auto& pend = pending_audio_events[i];

    if (curr_time < pend.time) {
      //  Not yet.
      continue;
    } else {
      to_remove.push_back(i);
    }

    if (pend.type == AudioEvent::Type::NewDFTFrame) {
      auto frame_it = pending_analysis_frames.find(pend.id);
      if (frame_it != pending_analysis_frames.end()) {
        update_info.spectrum_analyzer_frame_callback(frame_it->second);

        spectrum_analyzer->return_pending_spectrum(std::move(frame_it->second));
        pending_analysis_frames.erase(frame_it);
      } else {
//        GROVE_LOG_WARNING_CAPTURE_META("Unrecognized NewDFTFrame id.", logging_id());
      }
    }
  }

  erase_set(pending_audio_events, to_remove);
}

void read_audio_events(audio::EventUpdateContext& context, const audio::EventUpdateInfo& update_info) {
  auto& spectrum_analyzer = update_info.spectrum_analyzer;
  auto& pending_analysis_frames = context.pending_analysis_frames;
  auto& audio_core = update_info.audio_core;
  auto& ui_parameter_change_list = context.ui_parameter_change_list;
  auto& pending_audio_events = context.pending_audio_events;
  auto& temporary_events = context.temporary_audio_events;

  spectrum_analyzer->read_pending_spectra(pending_analysis_frames);

  if (audio_core.renderer.check_dropped_events()) {
    GROVE_LOG_WARNING_CAPTURE_META("Dropped some audio events.", logging_id());
    //  Any of the spectra we've previously read may be associated with an event id that was
    //  lost, so for now we just return all of them.
    for (auto& frame_it : pending_analysis_frames) {
      spectrum_analyzer->return_pending_spectrum(std::move(frame_it.second));
    }

    pending_analysis_frames.clear();
  }

  temporary_events.clear();
  audio_core.renderer.read_events(temporary_events);

  for (const auto& evts : temporary_events) {
    for (const auto& evt : evts) {
      if (evt.type == AudioEvent::Type::NewAudioParameterValue) {
        ui_parameter_change_list.parameter_change_events.push_back(evt);

      } else {
        pending_audio_events.push_back(evt);
      }
    }
  }
}

} //  anon

void audio::ui_initialize_events(EventUpdateContext& context) {
  context.temporary_audio_events.reserve(256);
#if GROVE_INCLUDE_NEW_EVENT_SYSTEM
  audio_event_system::ui_initialize();
#endif
}

void audio::ui_terminate_events(EventUpdateContext&) {
#if GROVE_INCLUDE_NEW_EVENT_SYSTEM
  audio_event_system::ui_terminate();
#endif
}

audio::EventUpdateResult audio::ui_process_events(EventUpdateContext& context,
                                                  const EventUpdateInfo& info) {
  audio::EventUpdateResult result{};
  clear_context(context);
  read_audio_events(context, info);
  update_audio_events(context, info);
#if GROVE_INCLUDE_NEW_EVENT_SYSTEM
  update_audio_event_system(context, info, result);
#endif
  return result;
}

GROVE_NAMESPACE_END
