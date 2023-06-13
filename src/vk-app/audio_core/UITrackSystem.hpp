#pragma once

#include "AudioComponent.hpp"

namespace grove {

struct UITrackSystemTrackHandle {
  uint32_t id;
  GROVE_INTEGER_IDENTIFIER_EQUALITY(UITrackSystemTrackHandle, id)
};

struct UITrackSystemTrack {
  enum class MIDIOutputSource {
    Triggered,
    NoteClipStateMachine,
    Arp
  };

  bool triggered_midi_output_enabled(const AudioComponent& component) const;
  bool ncsm_midi_output_enabled(const AudioComponent& component) const;
  bool arp_midi_output_enabled(const AudioComponent& component) const;

  UITrackSystemTrackHandle handle{};
  MIDIMessageStreamHandle midi_stream{};
  ArpeggiatorInstanceHandle arp{};
  int ncsm_voice_index{};
  UIMIDIMessageStreamNodes::List midi_stream_nodes;
};

struct UITrackSystem;

namespace track {

UITrackSystem* get_global_ui_track_system();

bool can_create_track(UITrackSystem* sys);
UITrackSystemTrackHandle create_track(
  UITrackSystem* sys, AudioComponent& component, const PitchSampleSetGroupHandle& pitch_sample_group);
void destroy_track(UITrackSystem* sys, UITrackSystemTrackHandle track, AudioComponent& component);
ArrayView<const UITrackSystemTrack> read_tracks(UITrackSystem* sys);
const UITrackSystemTrack* read_track(UITrackSystem* sys, UITrackSystemTrackHandle handle);
void end_update(UITrackSystem* sys, AudioComponent& component);

void toggle_midi_output_enabled(
  UITrackSystem* sys, AudioComponent& component,
  UITrackSystemTrackHandle track, UITrackSystemTrack::MIDIOutputSource source);

void set_midi_recording_track(UITrackSystem* sys, UITrackSystemTrackHandle track);
bool is_midi_recording_enabled(const UITrackSystem* sys);
void toggle_midi_recording_enabled(UITrackSystem* sys);

void note_on(UITrackSystem* sys, AudioComponent& component, MIDINote note);
void note_on_timeout(UITrackSystem* sys, AudioComponent& component, MIDINote note, float s);
void note_off(UITrackSystem* sys, AudioComponent& component, MIDINote note);

}

}