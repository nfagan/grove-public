#pragma once

#include "grove/audio/NoteClipStateMachineSystem.hpp"

namespace grove {

struct ControlNoteClipStateMachine;

struct ControlNoteClipStateMachineSectionRange {
public:
  int size() const {
    return end - begin;
  }

  int absolute_section_index(int i) const {
    return (i % (end - begin)) + begin;
  }

public:
  int begin;
  int end;
};

struct ReadControlNoteClipStateMachineVoice {
  int section_range_index;
};

} //  anon

namespace grove::ncsm {

ControlNoteClipStateMachine* get_global_control_note_clip_state_machine();
void initialize(ControlNoteClipStateMachine* control, NoteClipStateMachineSystem* sys);
void update(ControlNoteClipStateMachine* control, NoteClipStateMachineSystem* sys);

int get_num_sections_per_range(const ControlNoteClipStateMachine* control);
int get_num_section_ranges(const ControlNoteClipStateMachine* control);

ControlNoteClipStateMachineSectionRange get_section_range(
  const ControlNoteClipStateMachine* control, int ri);
ReadControlNoteClipStateMachineVoice read_voice(const ControlNoteClipStateMachine* control, int vi);

bool get_auto_advance(const ControlNoteClipStateMachine* control);
void set_auto_advance(ControlNoteClipStateMachine* control, bool value);

void set_next_section_index(
  ControlNoteClipStateMachine* control, NoteClipStateMachineSystem* sys, int vi, int si);
void set_section_range(
  ControlNoteClipStateMachine* control, NoteClipStateMachineSystem* sys, int vi, int ri);

int get_ui_section_range_index();
int get_environment_section_range_index();

}