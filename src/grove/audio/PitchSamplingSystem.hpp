#pragma once

#include "types.hpp"
#include "grove/common/identifier.hpp"

namespace grove {

struct PitchSamplingSystem;

struct PitchSampleSetGroupHandle {
  GROVE_INTEGER_IDENTIFIER_EQUALITY(PitchSampleSetGroupHandle, id)
  uint32_t id;
};

namespace pss {

PitchSamplingSystem* get_global_pitch_sampling_system();

void ui_initialize(PitchSamplingSystem* sys);
void ui_update(PitchSamplingSystem* sys);

PitchSampleSetGroupHandle ui_get_ith_group(const PitchSamplingSystem* sys, uint32_t i);
int ui_get_num_groups(const PitchSamplingSystem* sys);
int ui_get_num_sets_in_group(const PitchSamplingSystem* sys, PitchSampleSetGroupHandle group);

void ui_set_sample_set_from_semitones(
  PitchSamplingSystem* sys, PitchSampleSetGroupHandle group,
  uint32_t set, const float* sts, int num_notes);

void ui_push_triggered_semitones(
  PitchSamplingSystem* sys, PitchSampleSetGroupHandle group, uint32_t set,
  const float* sts, int num_notes);
void ui_push_triggered_note_numbers(
  PitchSamplingSystem* sys, PitchSampleSetGroupHandle group, uint32_t set,
  const uint8_t* note_nums, int num_notes, uint8_t ref_note_number);
void ui_push_triggered_notes(
  PitchSamplingSystem* sys, PitchSampleSetGroupHandle group, uint32_t set,
  const MIDINote* notes, int num_notes, MIDINote ref_note);

void ui_set_prefer_triggered_sample_set(
  PitchSamplingSystem* sys, PitchSampleSetGroupHandle group, uint32_t set, bool prefer_triggered);
bool ui_prefers_triggered_sample_set(
  PitchSamplingSystem* sys, PitchSampleSetGroupHandle group, uint32_t set);

MIDINote ui_uniform_sample_midi_note(
  const PitchSamplingSystem* sys, PitchSampleSetGroupHandle group, uint32_t set,
  int8_t base_octave, MIDINote dflt);

int ui_read_unique_pitch_classes_in_sample_set(
  const PitchSamplingSystem* sys, PitchSampleSetGroupHandle group, uint32_t set, PitchClass pcs[12]);

void render_begin_process(PitchSamplingSystem* sys, const AudioRenderInfo& info);
double render_uniform_sample_semitone(
  const PitchSamplingSystem* sys, PitchSampleSetGroupHandle group, uint32_t set, double dflt);
MIDINote render_uniform_sample_midi_note(
  const PitchSamplingSystem* sys, PitchSampleSetGroupHandle group, uint32_t set, int8_t base_oct);
[[nodiscard]] int render_read_semitones(
  const PitchSamplingSystem* sys, PitchSampleSetGroupHandle group, uint32_t set,
  double* dst, int max_num_dst);

}

}