#pragma once

#include "grove/math/vector.hpp"

namespace grove {
class AudioComponent;
class SelectedInstrumentComponents;
class KeyTrigger;
class MouseButtonTrigger;
struct UIAudioConnectionManager;
struct UITrackSystem;
struct RhythmParameters;
struct ControlNoteClipStateMachine;
}

namespace grove::pss {
struct PitchSamplingParameters;
}

namespace grove::gui {
namespace cursor {
struct CursorState;
}

struct RenderData;

} //  gui

namespace grove::ui {

enum class AudioEditorMode {
  Node = 0,
  Timeline,
  Track
};

struct AudioEditorCommonContext {
  AudioComponent& audio_component;
  UIAudioConnectionManager& ui_audio_connection_manager;
  UITrackSystem& ui_track_system;
  pss::PitchSamplingParameters& pitch_sampling_parameters;
  RhythmParameters& rhythm_parameters;
  ControlNoteClipStateMachine& control_note_clip_state_machine;
  const KeyTrigger& key_trigger;
  const MouseButtonTrigger& mouse_button_trigger;
  SelectedInstrumentComponents& selected;
  gui::cursor::CursorState& cursor_state;
  gui::RenderData& render_data;
  Vec2f container_dimensions;
  bool hidden;
  AudioEditorMode mode;
};

}