#include "UITimelineSystem.hpp"
#include "AudioConnectionManager.hpp"
#include "grove/audio/MIDIMessageStreamSystem.hpp"
#include "grove/audio/QuantizedTriggeredNotes.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using UniquePlayResult = std::unique_ptr<TriggeredNotes::PlayResult>;

UITimelineSystem::NoteClipTrackNode*
find_note_clip_track_node(UITimelineSystem* sys, TimelineNoteClipTrackHandle handle) {
  for (auto& node : sys->note_clip_tracks) {
    if (node.track_handle == handle) {
      return &node;
    }
  }
  return nullptr;
}

const UITimelineSystem::NoteClipTrackNode*
find_note_clip_track_node(const UITimelineSystem* sys, TimelineNoteClipTrackHandle handle) {
  for (auto& node : sys->note_clip_tracks) {
    if (node.track_handle == handle) {
      return &node;
    }
  }
  return nullptr;
}

UniquePlayResult require_play_result(UITimelineSystem* sys) {
  if (!sys->free_note_play_results.empty()) {
    auto res = std::move(sys->free_note_play_results.back());
    sys->free_note_play_results.pop_back();
    return res;
  } else {
    return std::make_unique<TriggeredNotes::PlayResult>();
  }
}

void return_play_result(UITimelineSystem* sys, UniquePlayResult result) {
  *result = {};
  sys->free_note_play_results.push_back(std::move(result));
}

void process_pending_recorded_notes(UITimelineSystem* ui_sys, TimelineSystem* sys) {
  auto& pending_results = ui_sys->pending_note_play_results;
  auto it = pending_results.begin();
  while (it != pending_results.end()) {
    auto& pend = *it;
    if (pend.result->is_ready) {
      if (ui_is_note_clip_track(sys, pend.track_handle) && !pend.result->played_span.empty()) {
        ClipNote note{};
        note.span = pend.result->played_span;
        note.note = pend.result->note;
        ui_maybe_insert_recorded_note(sys, pend.track_handle, note);
      }
      return_play_result(ui_sys, std::move(pend.result));
      it = pending_results.erase(it);
    } else {
      ++it;
    }
  }
}

} //  anon

TimelineAudioTrackHandle
UITimelineSystem::create_audio_track(TimelineSystem& timeline_system,
                                     AudioNodeStorage& node_storage) {
  auto handle = grove::ui_create_audio_track(&timeline_system);
  auto node_ctor = [handle, sys = &timeline_system](AudioNodeStorage::NodeID) {
    return new TimelineAudioTrackNode(sys, handle, 2);
  };

  AudioTrackNode node{};
  node.track_handle = handle;
  node.processor_node = node_storage.create_node(
    node_ctor, make_port_descriptors_from_audio_node_ctor(node_ctor));
  audio_tracks.push_back(node);

  ui_create_timeline_audio_clip(
    &timeline_system,
    handle,
    AudioBufferHandle{},
    ScoreRegion{{}, {2, 0.0}});

  ui_set_track_loop_region(
    &timeline_system,
    handle,
    ScoreRegion{{}, {2, 0.0}});

  return handle;
}

void UITimelineSystem::destroy_audio_track(TimelineAudioTrackHandle handle,
                                           TimelineSystem& sys,
                                           AudioConnectionManager& connection_manager) {
  auto it = std::find_if(audio_tracks.begin(), audio_tracks.end(), [handle](const auto& node) {
    return node.track_handle == handle;
  });
  if (it != audio_tracks.end()) {
    ui_destroy_audio_track(&sys, it->track_handle);
    (void) connection_manager.maybe_delete_node(it->processor_node);
    audio_tracks.erase(it);
  } else {
    assert(false);
  }
}

#define DEBUG_CLIP_TYPE (3)

TimelineNoteClipTrackHandle UITimelineSystem::create_note_clip_track(
  TimelineSystem& timeline_system, ArpeggiatorSystem& arp_sys,
  AudioNodeStorage& node_storage, const PitchSampleSetGroupHandle& pitch_sample_group) {
  //
  auto* midi_sys = timeline_system.midi_message_stream_system;
  MIDIMessageStreamHandle stream = midi::ui_create_stream(midi_sys);
  midi::ui_enable_source(midi_sys, stream, 1);  //  timeline system note clip
  midi::ui_enable_source(midi_sys, stream, 2);  //  triggered notes
//  midi::ui_enable_source(midi_sys, stream, qtn::ui_get_midi_source_id());  //  quantized triggered notes
  midi::ui_enable_source_note_onset_feedback(midi_sys, stream, 1);
#if 1
  auto arp = arp::ui_create_arpeggiator(&arp_sys, stream.id);
  const PitchClass pcs[5]{PitchClass(0), PitchClass(2), PitchClass(5), PitchClass(7), PitchClass(9)};
  const int8_t octs[3]{2, 3, 4};
  arp::ui_set_note_sampling_parameters(&arp_sys, arp, pcs, 5, octs, 3);

  MIDINote notes[4]{
    MIDINote{PitchClass(0), 3, 127},
    MIDINote{PitchClass(2), 3, 127},
    MIDINote{PitchClass(5), 3, 127},
    MIDINote{PitchClass(7), 3, 127},
  };

  arp::ui_set_note_cycling_parameters(&arp_sys, arp, notes, 4, 5, 7);
  arp::ui_set_pitch_sample_set_group(&arp_sys, arp, pitch_sample_group);
  arp::ui_set_pitch_mode(&arp_sys, arp, ArpeggiatorSystemPitchMode::RandomFromPitchSampleSet);
//  arp::ui_set_duration_mode(arp_sys, arp, ArpeggiatorSystemDurationMode::Sixteenth);
  arp::ui_set_num_active_slots(&arp_sys, arp, 4);
#endif

  const uint32_t stream_id = stream.id;

  auto handle = grove::ui_create_note_clip_track(&timeline_system, stream_id);
  auto node_ctor = [handle, sys = &timeline_system](AudioNodeStorage::NodeID) {
    return new TimelineNoteClipTrackNode(sys, handle);
  };

  NoteClipTrackNode node{};
  node.track_handle = handle;
  node.midi_stream_id = stream_id;
  node.arp = arp;
  for (int i = 0; i < 4; i++) {
    auto processor_node_id = node_storage.create_node(
      node_ctor, make_port_descriptors_from_audio_node_ctor(node_ctor));
    node.processor_nodes = note_clip_track_nodes.insert(node.processor_nodes, processor_node_id);
  }
  note_clip_tracks.push_back(node);

  ui_set_track_loop_region(
    &timeline_system,
    handle,
    ScoreRegion{{}, {4, 0.0}});

#if DEBUG_CLIP_TYPE == 3
  auto clip0 = ui_create_timeline_note_clip(
    &timeline_system,
    handle,
    ScoreRegion{{}, {4, 0.0}});
#else
  auto clip0 = ui_create_timeline_note_clip(
    &timeline_system,
    handle,
    ScoreRegion{{}, {1, 0.0}});

  auto clip1 = ui_create_timeline_note_clip(
    &timeline_system,
    handle,
    ScoreRegion{{1, 0.0}, {1, 0.0}});
#endif

#if DEBUG_CLIP_TYPE == 0
  ClipNote end_note{};
  end_note.note = MIDINote::C3;
  end_note.span.begin = ScoreCursor{0, 3.75};
  end_note.span.size = ScoreCursor{0, 0.251};
  add_note(timeline_system.clip_system, clip0, end_note);

  ClipNote next_note{};
  next_note.note = MIDINote::C3;
  next_note.span.begin = ScoreCursor{-1, 3.98};
  next_note.span.size = ScoreCursor{0, 2.0};
  add_note(timeline_system.clip_system, clip1, next_note);
#elif DEBUG_CLIP_TYPE == 1
  ClipNote dummy_note{};
  dummy_note.note = MIDINote::C3;
  dummy_note.span.begin = ScoreCursor{};
  dummy_note.span.size = ScoreCursor{0, 2.0};
  add_note(timeline_system.clip_system, clip0, dummy_note);

  dummy_note.note = MIDINote::A4;
  add_note(timeline_system.clip_system, clip0, dummy_note);
  add_note(timeline_system.clip_system, clip1, dummy_note);
#elif DEBUG_CLIP_TYPE == 2
  ClipNote dummy_note{};
  dummy_note.note = MIDINote::C3;
  dummy_note.span.begin = ScoreCursor{};
  dummy_note.span.size = ScoreCursor{1, 0.01};
  add_note(timeline_system.clip_system, clip0, dummy_note);

  dummy_note.note = MIDINote::A4;
  add_note(timeline_system.clip_system, clip0, dummy_note);
  add_note(timeline_system.clip_system, clip1, dummy_note);
#elif DEBUG_CLIP_TYPE == 3
  ClipNote dummy_note{};
  dummy_note.note = MIDINote::C3;
  dummy_note.span.begin = ScoreCursor{};
  dummy_note.span.size = ScoreCursor{1, 0.0};
  ui_add_note(timeline_system.clip_system, clip0, dummy_note);
#endif

  return handle;
}

void UITimelineSystem::set_recording_enabled(TimelineNoteClipTrackHandle handle, bool enabled) {
  if (auto* node = find_note_clip_track_node(this, handle)) {
    node->is_recording = enabled;
  } else {
    assert(false);
  }
}

void UITimelineSystem::toggle_recording_enabled(TimelineNoteClipTrackHandle handle) {
  if (auto* node = find_note_clip_track_node(this, handle)) {
    node->is_recording = !node->is_recording;
  } else {
    assert(false);
  }
}

void UITimelineSystem::toggle_midi_output_enabled(
  MIDIMessageStreamSystem& midi_stream_sys, TriggeredNotes& notes, TimelineNoteClipTrackHandle handle) {
  //
  if (auto* node = find_note_clip_track_node(this, handle)) {
    set_midi_output_enabled(midi_stream_sys, notes, handle, !node->midi_output_enabled);
  } else {
    assert(false);
  }
}

void UITimelineSystem::toggle_arp_enabled(
  TimelineSystem& timeline_system, ArpeggiatorSystem&, TimelineNoteClipTrackHandle handle) {
  //
  if (auto* node = find_note_clip_track_node(this, handle)) {
    MIDIMessageStreamHandle stream{node->midi_stream_id};
    const uint8_t source_id = arp::get_midi_source_id();
    if (node->arp_output_enabled) {
      node->arp_output_enabled = false;
      midi::ui_disable_source(timeline_system.midi_message_stream_system, stream, source_id);
    } else {
      node->arp_output_enabled = true;
      midi::ui_enable_source(timeline_system.midi_message_stream_system, stream, source_id);
    }
  } else {
    assert(false);
  }
}

void UITimelineSystem::set_midi_output_enabled(
  MIDIMessageStreamSystem& midi_stream_sys, TriggeredNotes& notes,
  TimelineNoteClipTrackHandle handle, bool enabled) {
  //
  auto* node = find_note_clip_track_node(this, handle);
  if (!node) {
    assert(false);
    return;
  }

  node->midi_output_enabled = enabled;
  if (!enabled) {
    notes::ui_flush_on(&notes, node->midi_stream_id);
  }

  MIDIMessageStreamHandle stream{node->midi_stream_id};
  const uint8_t qtn_source_id = qtn::ui_get_midi_source_id();
  if (node->midi_output_enabled) {
    midi::ui_enable_source(&midi_stream_sys, stream, qtn_source_id);
  } else {
    midi::ui_disable_source(&midi_stream_sys, stream, qtn_source_id);
  }
}

void UITimelineSystem::destroy_note_clip_track(
  TimelineNoteClipTrackHandle handle, TimelineSystem& timeline_system,
  TriggeredNotes& notes, ArpeggiatorSystem& arp_sys, AudioConnectionManager& connection_manager) {
  //
  auto it = std::find_if(note_clip_tracks.begin(), note_clip_tracks.end(), [handle](const auto& node) {
    return node.track_handle == handle;
  });

  if (it == note_clip_tracks.end()) {
    assert(false);
    return;
  }

  auto stream = MIDIMessageStreamHandle{it->midi_stream_id};
  midi::ui_destroy_stream(timeline_system.midi_message_stream_system, stream);
  ui_destroy_note_clip_track(&timeline_system, it->track_handle);
  notes::ui_flush_on(&notes, it->midi_stream_id);
  arp::ui_destroy_arpeggiator(&arp_sys, it->arp);

  auto node_it = note_clip_track_nodes.begin(it->processor_nodes);
  for (; node_it != note_clip_track_nodes.end(); ++node_it) {
    (void) connection_manager.maybe_delete_node(*node_it);
  }

  (void) note_clip_track_nodes.free_list(it->processor_nodes);
  note_clip_tracks.erase(it);
}

const UITimelineSystem::NoteClipTrackNode*
UITimelineSystem::read_note_clip_track_node(TimelineNoteClipTrackHandle handle) const {
  return find_note_clip_track_node(this, handle);
}

void UITimelineSystem::note_on(TriggeredNotes& notes, MIDINote note) {
  for (auto& track : note_clip_tracks) {
    if (track.midi_output_enabled) {
      notes::ui_note_on(&notes, track.midi_stream_id, note);
    }
  }
}

void UITimelineSystem::note_on_timeout(TriggeredNotes& notes, MIDINote note, float s) {
  for (auto& track : note_clip_tracks) {
    if (track.midi_output_enabled) {
      notes::ui_note_on_timeout(&notes, track.midi_stream_id, note, s);
    }
  }
}

void UITimelineSystem::note_off(TriggeredNotes& notes, MIDINote note) {
  for (auto& track : note_clip_tracks) {
    if (!track.midi_output_enabled) {
      continue;
    }

    if (!track.is_recording) {
      notes::ui_note_off(&notes, track.midi_stream_id, note);
    } else {
      auto res = require_play_result(this);
      if (notes::ui_note_off(&notes, track.midi_stream_id, note, res.get())) {
        PendingPlayResult pending{};
        pending.track_handle = track.track_handle;
        pending.result = std::move(res);
        pending_note_play_results.push_back(std::move(pending));
      } else {
        return_play_result(this, std::move(res));
      }
    }
  }
}

void UITimelineSystem::initialize(
  TimelineSystem& sys, NoteClipSystem& clip_sys,
  MIDIMessageStreamSystem& midi_sys, const Transport* transport,
  const AudioBufferStore* buffer_store) {
  //
  ui_initialize(&sys, &clip_sys, &midi_sys, transport, buffer_store);
}

void UITimelineSystem::end_update(TimelineSystem& sys) {
  process_pending_recorded_notes(this, &sys);
  ui_update(&sys);
}

GROVE_NAMESPACE_END
