#include "UITrackSystem.hpp"
#include "grove/common/common.hpp"
#include <vector>

GROVE_NAMESPACE_BEGIN

struct Config {
  static constexpr int max_num_tracks = 16;
};

using UniquePlayResult = std::unique_ptr<TriggeredNotes::PlayResult>;

struct PendingPlayResult {
  UITrackSystemTrackHandle track_handle;
  std::unique_ptr<TriggeredNotes::PlayResult> result;
};

struct UITrackSystem {
public:
  UITrackSystemTrack* find_track(UITrackSystemTrackHandle handle) {
    for (auto& track : tracks) {
      if (track.handle == handle) {
        return &track;
      }
    }
    return nullptr;
  }

  bool is_midi_recording_track(UITrackSystemTrackHandle handle) const {
    return midi_recording_track && midi_recording_track.value() == handle;
  }

public:
  std::vector<UITrackSystemTrack> tracks;
  uint32_t next_track_id{1};

  std::vector<std::unique_ptr<TriggeredNotes::PlayResult>> free_note_play_results;
  std::vector<PendingPlayResult> pending_note_play_results;

  bool midi_recording_enabled{};
  Optional<UITrackSystemTrackHandle> midi_recording_track;
};

namespace {

UniquePlayResult require_play_result(UITrackSystem* sys) {
  if (!sys->free_note_play_results.empty()) {
    auto res = std::move(sys->free_note_play_results.back());
    sys->free_note_play_results.pop_back();
    return res;
  } else {
    return std::make_unique<TriggeredNotes::PlayResult>();
  }
}

void return_play_result(UITrackSystem* sys, UniquePlayResult result) {
  *result = {};
  sys->free_note_play_results.push_back(std::move(result));
}

void process_pending_recorded_notes(UITrackSystem* sys, AudioComponent& audio_component) {
  auto* ncsm_sys = audio_component.get_note_clip_state_machine_system();
  auto* clip_sys = audio_component.get_note_clip_system();

  auto& pending_results = sys->pending_note_play_results;
  auto it = pending_results.begin();
  while (it != pending_results.end()) {
    auto& pend = *it;
    if (!pend.result->is_ready) {
      ++it;
      continue;
    }

    auto* target_track = sys->find_track(pend.track_handle);
    if (target_track && !pend.result->played_span.empty()) {
      ClipNote note{};
      note.span = pend.result->played_span;
      note.note = pend.result->note;
      ncsm::ui_maybe_insert_recorded_note(ncsm_sys, clip_sys, target_track->ncsm_voice_index, note);
    }

    return_play_result(sys, std::move(pend.result));
    it = pending_results.erase(it);
  }
}

struct {
  UITrackSystem sys;
} globals;

} //  anon

UITrackSystem* track::get_global_ui_track_system() {
  return &globals.sys;
}

bool track::can_create_track(UITrackSystem* sys) {
  return int(sys->tracks.size()) < Config::max_num_tracks;
}

UITrackSystemTrackHandle track::create_track(
  UITrackSystem* sys, AudioComponent& component, const PitchSampleSetGroupHandle& pitch_sample_group) {
  //
  assert(can_create_track(sys));

  UITrackSystemTrackHandle result{sys->next_track_id++};

  auto* note_clip_sm = component.get_note_clip_state_machine_system();
  auto* arp_sys = component.get_arpeggiator_system();

  auto& track = sys->tracks.emplace_back();
  track.handle = result;
  track.midi_stream = midi::ui_create_stream(component.get_midi_message_stream_system());
  track.arp = arp::ui_create_arpeggiator(arp_sys, track.midi_stream.id);
  track.ncsm_voice_index = ncsm::ui_acquire_next_voice(note_clip_sm, track.midi_stream);
  track.midi_stream_nodes = component.get_ui_midi_message_stream_nodes()->create(
    4, track.midi_stream, component.audio_node_storage);

  midi::ui_enable_source(
    component.get_midi_message_stream_system(), track.midi_stream, ncsm::get_midi_source_id());

  arp::ui_set_pitch_sample_set_group(arp_sys, track.arp, pitch_sample_group);
  arp::ui_set_pitch_mode(arp_sys, track.arp, ArpeggiatorSystemPitchMode::RandomFromPitchSampleSet);
  arp::ui_set_num_active_slots(arp_sys, track.arp, 4);

  return result;
}

void track::destroy_track(
  UITrackSystem* sys, UITrackSystemTrackHandle track_handle, AudioComponent& component) {
  //
  if (auto* track = sys->find_track(track_handle)) {
    midi::ui_destroy_stream(component.get_midi_message_stream_system(), track->midi_stream);
    arp::ui_destroy_arpeggiator(component.get_arpeggiator_system(), track->arp);
    ncsm::ui_return_voice(component.get_note_clip_state_machine_system(), track->ncsm_voice_index);
    component.get_ui_midi_message_stream_nodes()->destroy(
      track->midi_stream_nodes, component.audio_connection_manager);
    sys->tracks.erase(sys->tracks.begin() + (track - sys->tracks.data()));

    if (sys->is_midi_recording_track(track_handle)) {
      sys->midi_recording_track = NullOpt{};
    }
  }
}

const UITrackSystemTrack* track::read_track(UITrackSystem* sys, UITrackSystemTrackHandle handle) {
  return sys->find_track(handle);
}

ArrayView<const UITrackSystemTrack> track::read_tracks(UITrackSystem* sys) {
  return make_view(sys->tracks);
}

void track::note_on(UITrackSystem* sys, AudioComponent& component, MIDINote note) {
  for (auto& track : sys->tracks) {
    if (track.triggered_midi_output_enabled(component)) {
      notes::ui_note_on(component.get_triggered_notes(), track.midi_stream.id, note);
    }
  }
}

void track::note_on_timeout(UITrackSystem* sys, AudioComponent& component, MIDINote note, float s) {
  for (auto& track : sys->tracks) {
    if (track.triggered_midi_output_enabled(component)) {
      notes::ui_note_on_timeout(component.get_triggered_notes(), track.midi_stream.id, note, s);
    }
  }
}

void track::set_midi_recording_track(UITrackSystem* sys, UITrackSystemTrackHandle track) {
  sys->midi_recording_track = track;
}

void track::toggle_midi_recording_enabled(UITrackSystem* sys) {
  sys->midi_recording_enabled = !sys->midi_recording_enabled;
}

bool track::is_midi_recording_enabled(const UITrackSystem* sys) {
  return sys->midi_recording_enabled;
}

void track::toggle_midi_output_enabled(
  UITrackSystem* sys, AudioComponent& component, UITrackSystemTrackHandle track_handle,
  UITrackSystemTrack::MIDIOutputSource source) {
  //
  auto* track = sys->find_track(track_handle);
  if (!track) {
    return;
  }

  uint8_t source_id{};
  switch (source) {
    case UITrackSystemTrack::MIDIOutputSource::Triggered: {
      source_id = notes::get_triggered_notes_midi_source_id();
      break;
    }
    case UITrackSystemTrack::MIDIOutputSource::NoteClipStateMachine: {
      source_id = ncsm::get_midi_source_id();
      break;
    }
    case UITrackSystemTrack::MIDIOutputSource::Arp: {
      source_id = arp::get_midi_source_id();
      break;
    }
    default: {
      assert(false);
    }
  }

  auto* midi_sys = component.get_midi_message_stream_system();
  bool enabled = midi::ui_is_source_enabled(midi_sys, track->midi_stream, source_id);
  midi::ui_set_source_enabled(midi_sys, track->midi_stream, source_id, !enabled);

  if (source == UITrackSystemTrack::MIDIOutputSource::Triggered) {
    auto qtn_source_id = qtn::ui_get_midi_source_id();
    midi::ui_set_source_enabled(midi_sys, track->midi_stream, qtn_source_id, !enabled);
  }
}

void track::note_off(UITrackSystem* sys, AudioComponent& component, MIDINote note) {
  for (auto& track : sys->tracks) {
    if (!track.triggered_midi_output_enabled(component)) {
      continue;
    }

    auto* triggered_notes = component.get_triggered_notes();
    if (!sys->midi_recording_enabled || !sys->is_midi_recording_track(track.handle)) {
      notes::ui_note_off(triggered_notes, track.midi_stream.id, note);
    } else {
      auto res = require_play_result(sys);
      if (notes::ui_note_off(triggered_notes, track.midi_stream.id, note, res.get())) {
        PendingPlayResult pending{};
        pending.track_handle = track.handle;
        pending.result = std::move(res);
        sys->pending_note_play_results.push_back(std::move(pending));
      } else {
        return_play_result(sys, std::move(res));
      }
    }
  }
}

void track::end_update(UITrackSystem* sys, AudioComponent& component) {
  process_pending_recorded_notes(sys, component);
}

bool UITrackSystemTrack::triggered_midi_output_enabled(const AudioComponent& component) const {
  return midi::ui_is_source_enabled(
    component.get_midi_message_stream_system(),
    midi_stream, notes::get_triggered_notes_midi_source_id());
}

bool UITrackSystemTrack::arp_midi_output_enabled(const AudioComponent& component) const {
  return midi::ui_is_source_enabled(
    component.get_midi_message_stream_system(), midi_stream, arp::get_midi_source_id());
}

bool UITrackSystemTrack::ncsm_midi_output_enabled(const AudioComponent& component) const {
  return midi::ui_is_source_enabled(
    component.get_midi_message_stream_system(), midi_stream, ncsm::get_midi_source_id());
}

GROVE_NAMESPACE_END
