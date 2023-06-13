#pragma once

#include "UIAudioParameterManager.hpp"
#include "grove/audio/audio_events.hpp"
#include "grove/audio/audio_effects/SpectrumAnalyzer.hpp"
#include <vector>
#include <unordered_map>

namespace grove {

class AudioCore;
class SpectrumAnalyzer;

}

namespace grove::audio {

using SpectrumAnalyzerFrameCallback =
  std::function<void(const SpectrumAnalyzer::AnalysisFrame& frame)>;

struct EventUpdateContext {
  std::vector<AudioEvent> pending_audio_events;
  std::vector<AudioEvents> temporary_audio_events;
  std::unordered_map<uint32_t, SpectrumAnalyzer::AnalysisFrame> pending_analysis_frames;
  UIParameterChangeList ui_parameter_change_list;
  std::vector<uint32_t> new_render_buffer_event_ids;
};

struct EventUpdateInfo {
  bool is_stream_started;
  AudioCore& audio_core;
  SpectrumAnalyzer* spectrum_analyzer;
  const SpectrumAnalyzerFrameCallback& spectrum_analyzer_frame_callback;
};

struct EventUpdateResult {
  bool any_event_system_dropped_events;
};

void ui_initialize_events(EventUpdateContext& context);
void ui_terminate_events(EventUpdateContext& context);
EventUpdateResult ui_process_events(EventUpdateContext& context, const EventUpdateInfo& info);

}