#include "environment_global_sound_control.hpp"
#include "../audio_core/control_note_clip_state_machine.hpp"
#include "../audio_core/pitch_sampling.hpp"
#include "../audio_core/AudioComponent.hpp"
#include "../weather/common.hpp"
#include "grove/math/random.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include "grove/common/Stopwatch.hpp"
#include <imgui/imgui.h>

GROVE_NAMESPACE_BEGIN

namespace env {

enum class GlobalSoundEventState {
  Idle = 0,
  WantStart,
  Active
};

struct ControlNoteClipStateMachineEvent {
  enum class State {
    Idle = 0,
    TransitioningToActive,
    Active,
    TransitioningToInactive,
  };

  int src_nsi{};
  double src_bpm{};
  State state{};
  Stopwatch state_timer;
  float duration{128.0f};
  uint32_t event_count{};
};

struct GlobalSoundControl {
  bool auto_init_event{true};
  bool can_begin_event{true};
  bool need_begin_event{};
  bool allow_begin_event{};
  Optional<weather::State> began_by_weather_state{};
  GlobalSoundEventState state{};
  ControlNoteClipStateMachineEvent control_ncsm_event{};
  Stopwatch state_timer;
};

} //  env

namespace {

using namespace env;
using UpdateInfo = env::GlobalSoundControlUpdateInfo;

void set_ncsm_voice_section_ranges(GlobalSoundControl*, int ri, const UpdateInfo& info) {
  auto* ncsm_sys = info.audio_component.get_note_clip_state_machine_system();
  auto* control_ncsm_sys = &info.control_ncsm;

  const int nv = ncsm::ui_get_num_voices(ncsm_sys);
  for (int vi = 0; vi < nv; vi++) {
    ncsm::set_section_range(control_ncsm_sys, ncsm_sys, vi, ri);
  }
}

void restore_ncsm_parameters(GlobalSoundControl* control, const UpdateInfo& info) {
  info.pitch_sampling_params.set_primary_note_set_index(
    info.audio_component.get_pitch_sampling_system(),
    info.audio_component.get_audio_scale_system(),
    control->control_ncsm_event.src_nsi);
  //  more fun to keep the (possibly) modified bpm but restore the original ncsm composition
//  info.audio_component.audio_transport.set_bpm(control->control_ncsm_event.src_bpm);
}

void start_ncsm(GlobalSoundControl*, const UpdateInfo& info) {
  auto* ncsm_sys = info.audio_component.get_note_clip_state_machine_system();
  auto* control_ncsm_sys = &info.control_ncsm;

  ncsm::set_auto_advance(control_ncsm_sys, true);
  if (!ncsm::ui_send_next_section_indices_sync(ncsm_sys, 8e-3f)) {
    GROVE_LOG_WARNING_CAPTURE_META("Failed to send next section indices in time.", "start_ncsm");
  }

  if (!info.audio_component.audio_transport.ui_playing()) {
    info.audio_component.audio_transport.toggle_play_stop();
  }
}

double choose_ncsm_event_bpm(double src_bpm) {
  const int num_bpms = 4;
  double poss_bpms[num_bpms]{75.0, 80.0, 90.0, 120.0};
  int poss_bpmi[num_bpms];
  poss_bpmi[0] = 0;
  int num_poss_bpms{};
  for (int i = 0; i < num_bpms; i++) {
    if (poss_bpms[i] != src_bpm) {
      poss_bpmi[num_poss_bpms++] = i;
    }
  }
  num_poss_bpms = std::max(1, num_poss_bpms);
  int bpmi = *uniform_array_sample(poss_bpmi, num_poss_bpms);
  return poss_bpms[bpmi];
}

void prepare_ncsm_event(GlobalSoundControl* control, const UpdateInfo& info) {
  auto* ncsm_sys = info.audio_component.get_note_clip_state_machine_system();
  auto* control_ncsm_sys = &info.control_ncsm;
  auto* note_clip_sys = info.audio_component.get_note_clip_system();
  auto* pitch_sample_sys = info.audio_component.get_pitch_sampling_system();
  auto* scale_sys = info.audio_component.get_audio_scale_system();
  auto* transport = &info.audio_component.audio_transport;

  const int ri = ncsm::get_environment_section_range_index();
  const auto section_range = ncsm::get_section_range(control_ncsm_sys, ri);
  const int nv = ncsm::ui_get_num_voices(ncsm_sys);

  {
    //  store original bpm
    const double src_bpm = transport->get_bpm();
    control->control_ncsm_event.src_bpm = src_bpm;
    transport->set_bpm(choose_ncsm_event_bpm(src_bpm));
  }

  //  store original nsi
  control->control_ncsm_event.src_nsi = info.pitch_sampling_params.primary_note_set_index;
  //  set new nsi
  int nsi{};
  int nsi_index = int(control->control_ncsm_event.event_count % 3);
  switch (nsi_index) {
    case 0:
      nsi = info.pitch_sampling_params.get_pentatonic_major_note_set_index();
      break;
    case 1:
      nsi = info.pitch_sampling_params.get_lydian_e_note_set_index();
      break;
    case 2:
      nsi = info.pitch_sampling_params.get_minor_key1_note_set_index();
      break;
  }
  info.pitch_sampling_params.set_primary_note_set_index(pitch_sample_sys, scale_sys, nsi);

  float sts[pss::PitchSamplingParameters::max_num_notes];
  int num_sts{};
  info.pitch_sampling_params.get_note_set(scale_sys, nsi, sts, &num_sts);
  assert(num_sts > 0);

  ScoreCursor clip_sizes[3]{
    ScoreCursor{1, 0},
    ScoreCursor{2, 0},
    ScoreCursor{4, 0},
  };

  double beat_event_intervals[6]{
    1.0, 1.0, 1.0, 0.5, 0.5, 0.25,
  };

  double p_rests[5]{0.125, 0.125, 0.125, 0.5, 0.75};

  const double tsig_num = reference_time_signature().numerator;

  for (int vi = 0; vi < nv; vi++) {
    for (int si = section_range.begin; si < section_range.end; si++) {
      auto read_section = ncsm::ui_read_section(ncsm_sys, si);
#if 1
      auto clip_size = *uniform_array_sample(clip_sizes, 3);
      const double p_rest = *uniform_array_sample(p_rests, 5);
      double event_isi = *uniform_array_sample(beat_event_intervals, 6);
      ui_randomize_clip_contents(
        note_clip_sys, read_section.clip_handle, clip_size, tsig_num, p_rest, event_isi, sts, num_sts);
#else
      ui_remove_all_notes(note_clip_sys, read_section.clip_handle);

      auto clip_size = *uniform_array_sample(clip_sizes, 3);
      auto clip_size_beats = clip_size.to_beats(tsig_num);

      double event_isi = *uniform_array_sample(beat_event_intervals, 6);
      int num_events = std::max(1, int(clip_size_beats / event_isi));

      const double p_rest = *uniform_array_sample(p_rests, 5);
      for (int e = 0; e < num_events; e++) {
        if (urand() < p_rest) {
          continue;
        }

        const float st = *uniform_array_sample(sts, num_sts);

        ScoreCursor start = ScoreCursor::from_beats(event_isi * double(e), tsig_num);
        ScoreCursor end = start;
        end.wrapped_add_beats(event_isi, tsig_num);

        ClipNote note{};
        note.span = ScoreRegion::from_begin_end(start, end, tsig_num);
        note.note = c_relative_semitone_offset_to_midi_note(int(st), 3, 127);
        ui_add_note(note_clip_sys, read_section.clip_handle, note);
      }
#endif
    }
  }
}

auto update_control_ncsm_event(GlobalSoundControl* control, const UpdateInfo& info) {
  using State = ControlNoteClipStateMachineEvent::State;

  struct Result {
    bool finished;
  };

  Result result{};

  switch (control->control_ncsm_event.state) {
    case State::Idle: {
      prepare_ncsm_event(control, info);
      control->control_ncsm_event.state = State::TransitioningToActive;
      break;
    }
    case State::TransitioningToActive: {
      set_ncsm_voice_section_ranges(control, ncsm::get_environment_section_range_index(), info);
      start_ncsm(control, info);
      control->control_ncsm_event.state = State::Active;
      control->state_timer.reset();
      break;
    }
    case State::Active: {
      //  determine whether to inactivate.
      bool elapsed_cond;
      if (control->began_by_weather_state) {
        elapsed_cond = info.weather_status.frac_next == 0.0f &&
          info.weather_status.current != control->began_by_weather_state.value();
      } else {
        auto elapsed = control->state_timer.delta().count();
        elapsed_cond = elapsed >= control->control_ncsm_event.duration;
      }
      if (elapsed_cond) {
        control->control_ncsm_event.state = State::TransitioningToInactive;
      }
      break;
    }
    case State::TransitioningToInactive: {
      set_ncsm_voice_section_ranges(control, ncsm::get_ui_section_range_index(), info);
      restore_ncsm_parameters(control, info);
      control->control_ncsm_event.state = State::Idle;
      control->control_ncsm_event.event_count++;
      result.finished = true;
      break;
    }
  }

  return result;
}

bool auto_init_event_by_weather(
  const GlobalSoundControl*, const UpdateInfo& info, weather::State* began_by) {
  //
  bool res = info.weather_status.current == weather::State::Overcast &&
             info.weather_status.frac_next == 0.0f;

  if (!res) {
    return false;
  }

  *began_by = weather::State::Overcast;
  return true;
}

struct {
  env::GlobalSoundControl control{};
} globals;

} //  anon

env::GlobalSoundControl* env::get_global_global_sound_control() {
  return &globals.control;
}

void env::begin_update(GlobalSoundControl* control, const UpdateInfo& info) {
  //  1. idle -> want start event
  //  2. (allow_start_event()). want start event -> active

  if (control->state == GlobalSoundEventState::Idle && control->can_begin_event) {
    weather::State began_by_state{};
    if (control->auto_init_event && auto_init_event_by_weather(control, info, &began_by_state)) {
      control->need_begin_event = true;
      control->allow_begin_event = true;
      control->began_by_weather_state = began_by_state;
    }
  }

  double state_elapsed = control->state_timer.delta().count();
  switch (control->state) {
    case GlobalSoundEventState::Idle: {
      if (control->need_begin_event) {
        assert(control->can_begin_event);
        control->state = GlobalSoundEventState::WantStart;
        control->need_begin_event = false;
        control->can_begin_event = false;
      } else {
        control->can_begin_event = state_elapsed > 8.0;
      }
      break;
    }
    case GlobalSoundEventState::WantStart: {
      if (control->allow_begin_event) {
        control->allow_begin_event = false;
        control->state = GlobalSoundEventState::Active;
      }
      break;
    }
    case GlobalSoundEventState::Active: {
      auto res = update_control_ncsm_event(control, info);
      if (res.finished) {
        control->state = GlobalSoundEventState::Idle;
        control->state_timer.reset();
        control->began_by_weather_state = NullOpt{};
      }
      break;
    }
  }
}

void env::render_debug_gui(GlobalSoundControl* control) {
  ImGui::Begin("DebugGlobalSoundControl");

  if (control->state == GlobalSoundEventState::Idle && control->can_begin_event) {
    if (ImGui::Button("InitEvent")) {
      control->need_begin_event = true;
    }
  } else if (control->state == GlobalSoundEventState::WantStart) {
    if (ImGui::Button("AllowEvent")) {
      control->allow_begin_event = true;
    }
  }

  ImGui::SliderFloat("NCSMEventDuration", &control->control_ncsm_event.duration, 0.0f, 128.0f);
  ImGui::End();
}

GROVE_NAMESPACE_END
