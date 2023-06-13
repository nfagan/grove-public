#pragma once

namespace grove {
class SelectedInstrumentComponents;
class AudioComponent;
struct ControlNoteClipStateMachine;
}

namespace grove::debug {

struct DebugNotClipStateMachineContext {
  AudioComponent& audio_component;
  SelectedInstrumentComponents& selected;
  ControlNoteClipStateMachine& control_ncsm;
};

void render_debug_note_clip_state_machine_gui(const DebugNotClipStateMachineContext& context);

}