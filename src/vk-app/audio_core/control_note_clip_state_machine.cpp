#include "control_note_clip_state_machine.hpp"
#include "grove/common/common.hpp"
#include "grove/math/util.hpp"
#include "grove/math/random.hpp"

GROVE_NAMESPACE_BEGIN

struct Config {
  static constexpr int num_section_ranges = 2;
};

struct Voice {
  int ncsm_vi;
  int section_range_index;
  int min_section_repetitions;
};

struct ControlNoteClipStateMachine {
  DynamicArray<Voice, 16> voices;
  ControlNoteClipStateMachineSectionRange section_ranges[Config::num_section_ranges]{};
  int num_sections_per_range{};
  bool auto_advance{};
};

namespace {

struct {
  ControlNoteClipStateMachine sys;
} globals;

int get_num_section_repetitions() {
  return int(lerp(urand(), 4.0, 20.0));
}

} //  anon

ControlNoteClipStateMachine* ncsm::get_global_control_note_clip_state_machine() {
  return &globals.sys;
}

void ncsm::initialize(ControlNoteClipStateMachine* control, NoteClipStateMachineSystem* sys) {
  const int num_voices = ncsm::ui_get_num_voices(sys);
  control->voices.resize(num_voices);

  int vi{};
  for (auto& voice : control->voices) {
    voice.ncsm_vi = vi++;
    voice.section_range_index = 0;
    voice.min_section_repetitions = get_num_section_repetitions();
  }

  const int num_sections = ncsm::ui_get_num_sections(sys);
  assert(num_sections > 0 && num_sections % 2 == 0);

  control->section_ranges[0].begin = 0;
  control->section_ranges[0].end = num_sections / 2;
  control->section_ranges[1].begin = num_sections / 2;
  control->section_ranges[1].end = num_sections;
  control->num_sections_per_range = num_sections / 2;
}

void ncsm::update(ControlNoteClipStateMachine* control, NoteClipStateMachineSystem* sys) {
  if (control->auto_advance) {
    for (auto& voice : control->voices) {
      auto sys_voice = ncsm::ui_read_voice(sys, voice.ncsm_vi);
      if (sys_voice.num_section_repetitions < voice.min_section_repetitions) {
        continue;
      }

      auto range = ncsm::get_section_range(control, voice.section_range_index);
      const int curr_si = range.absolute_section_index(sys_voice.section) - range.begin;
      const int next_si = range.absolute_section_index(curr_si + 1);
      ncsm::ui_set_next_section_index(sys, voice.ncsm_vi, next_si);

      voice.min_section_repetitions = get_num_section_repetitions();
    }
  }
}

int ncsm::get_num_sections_per_range(const ControlNoteClipStateMachine* control) {
  auto res = control->num_sections_per_range;
  assert(res > 0);
  return res;
}

int ncsm::get_num_section_ranges(const ControlNoteClipStateMachine*) {
  return Config::num_section_ranges;
}

ControlNoteClipStateMachineSectionRange ncsm::get_section_range(
  const ControlNoteClipStateMachine* control, int ri) {
  //
  assert(ri >= 0 && ri < Config::num_section_ranges);
  return control->section_ranges[ri];
}

ReadControlNoteClipStateMachineVoice ncsm::read_voice(
  const ControlNoteClipStateMachine* control, int vi) {
  //
  auto& ctrl_voice = control->voices[vi];
  ReadControlNoteClipStateMachineVoice read{};
  read.section_range_index = ctrl_voice.section_range_index;
  return read;
}

void ncsm::set_auto_advance(ControlNoteClipStateMachine* control, bool value) {
  control->auto_advance = value;
}

bool ncsm::get_auto_advance(const ControlNoteClipStateMachine* control) {
  return control->auto_advance;
}

void ncsm::set_next_section_index(
  ControlNoteClipStateMachine* control, NoteClipStateMachineSystem* sys, int vi, int si) {
  //
  auto& voice = control->voices[vi];
  assert(si >= 0 && si < control->num_sections_per_range);
  auto& rng = control->section_ranges[voice.section_range_index];
  ncsm::ui_set_next_section_index(sys, vi, rng.begin + si);
}

void ncsm::set_section_range(
  ControlNoteClipStateMachine* control, NoteClipStateMachineSystem* sys, int vi, int ri) {
  //
  assert(ri >= 0 && ri < Config::num_section_ranges);

  auto& voice = control->voices[vi];
  if (voice.section_range_index != ri) {
    auto& curr_range = control->section_ranges[voice.section_range_index];
    auto& next_range = control->section_ranges[ri];

    auto read_voice = ncsm::ui_read_voice(sys, vi);
    int abs_si = read_voice.section % (curr_range.end - curr_range.begin);
    ncsm::ui_set_next_section_index(sys, vi, abs_si + next_range.begin);

    voice.section_range_index = ri;
  }
}

int ncsm::get_ui_section_range_index() {
  return 0;
}

int ncsm::get_environment_section_range_index() {
  return 1;
}

GROVE_NAMESPACE_END
