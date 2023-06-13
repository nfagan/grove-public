#pragma once

#include "NoteClipSystem.hpp"

namespace grove {

class Transport;
struct MIDIMessageStreamSystem;
struct MIDIMessageStreamHandle;
struct AudioRenderInfo;

struct NoteClipStateMachineSystem;
struct NoteClipStateMachineVoiceHandle {
  uint32_t id;
};

struct NoteClipStateMachineReadSection {
  NoteClipHandle clip_handle;
};

struct NoteClipStateMachineReadVoice {
  int section;
  Optional<int> next_section;
  int num_section_repetitions;
  ScoreCursor position;
};

namespace ncsm {

NoteClipStateMachineSystem* get_global_note_clip_state_machine();
uint8_t get_midi_source_id();

void render_begin_process(NoteClipStateMachineSystem* sys, const AudioRenderInfo& info);

void ui_initialize(
  NoteClipStateMachineSystem* sys, const Transport* transport, NoteClipSystem* clip_sys,
  MIDIMessageStreamSystem* midi_stream_sys);
void ui_update(NoteClipStateMachineSystem* sys);

void ui_set_next_section_index(NoteClipStateMachineSystem* sys, int vi, int si);
bool ui_send_next_section_indices_sync(NoteClipStateMachineSystem* sys, float timeout);

int ui_get_num_voices(NoteClipStateMachineSystem* sys);
int ui_get_num_sections(NoteClipStateMachineSystem* sys);

NoteClipStateMachineReadSection ui_read_section(NoteClipStateMachineSystem* sys, int si);
NoteClipStateMachineReadVoice ui_read_voice(NoteClipStateMachineSystem* sys, int vi);
void ui_maybe_insert_recorded_note(
  NoteClipStateMachineSystem* sys, NoteClipSystem* clip_sys, int vi, const ClipNote& note);

int ui_acquire_next_voice(NoteClipStateMachineSystem* sys, const MIDIMessageStreamHandle& stream);
void ui_return_voice(NoteClipStateMachineSystem* sys, int vi);

}

}