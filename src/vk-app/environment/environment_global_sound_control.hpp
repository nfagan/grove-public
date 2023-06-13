#pragma once

namespace grove {
class AudioComponent;
struct ControlNoteClipStateMachine;

namespace pss {
struct PitchSamplingParameters;
}

namespace weather {
struct Status;
}

}

namespace grove::env {

struct GlobalSoundControl;

struct GlobalSoundControlUpdateInfo {
  AudioComponent& audio_component;
  ControlNoteClipStateMachine& control_ncsm;
  pss::PitchSamplingParameters& pitch_sampling_params;
  const weather::Status& weather_status;
};

GlobalSoundControl* get_global_global_sound_control();
void begin_update(GlobalSoundControl* control, const GlobalSoundControlUpdateInfo& info);

void render_debug_gui(GlobalSoundControl* control);

}