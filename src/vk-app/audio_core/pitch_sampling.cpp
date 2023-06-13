#include "pitch_sampling.hpp"
#include "note_sets.hpp"
#include "grove/audio/PitchSamplingSystem.hpp"
#include "grove/audio/AudioScaleSystem.hpp"
#include "grove/math/util.hpp"

namespace grove::pss {

using GetNoteSet = int(float[32]);

namespace {

GetNoteSet* global_sets[5]{
  notes::ui_get_note_set0,
  notes::ui_get_pentatonic_major_note_set,
  notes::ui_get_note_set1,
  notes::ui_get_note_set2,
  notes::ui_get_note_set3,
};

int get_note_set(const AudioScaleSystem* scale_sys, float* sts, int* num_sts, int nsi) {
  const int min_nsi = PitchSamplingParameters::min_note_set_index();
  const int max_nsi = PitchSamplingParameters::max_note_set_index();
  nsi = clamp(nsi, min_nsi, max_nsi);

  if (nsi == 0) {
    //  prefer 0, +/- octave
    auto scale_desc = scale_system::ui_get_ith_active_scale_desc(scale_sys, 0);

    int num_offsets{};
    sts[num_offsets++] = 0.0f;
    sts[num_offsets++] = -float(scale_desc.num_notes_per_octave);
    sts[num_offsets++] = float(scale_desc.num_notes_per_octave);
    *num_sts = num_offsets;

  } else {
    *num_sts = global_sets[nsi](sts);
  }

  return nsi;
}

int set_note_set(PitchSamplingSystem* sys, AudioScaleSystem* scale_sys, uint32_t group_index, int nsi) {
  float sts[notes::max_num_notes];
  int num_sts{};
  nsi = get_note_set(scale_sys, sts, &num_sts, nsi);

  auto group_handle = pss::ui_get_ith_group(sys, group_index);
  pss::ui_set_sample_set_from_semitones(sys, group_handle, 0, sts, num_sts);
  return nsi;
}

} //  anon

int PitchSamplingParameters::min_note_set_index() {
  return 0;
}

int PitchSamplingParameters::max_note_set_index() {
  return 4;
}

PitchSampleSetGroupHandle
PitchSamplingParameters::get_primary_group_handle(PitchSamplingSystem* sys) const {
  return pss::ui_get_ith_group(sys, primary_pitch_sample_group_index);
}

PitchSampleSetGroupHandle
PitchSamplingParameters::get_secondary_group_handle(PitchSamplingSystem* sys) const {
  return pss::ui_get_ith_group(sys, secondary_pitch_sample_group_index);
}

int PitchSamplingParameters::get_lydian_e_note_set_index() {
  return 4;
}

int PitchSamplingParameters::get_minor_key1_note_set_index() {
  return 2;
}

int PitchSamplingParameters::get_pentatonic_major_note_set_index() {
  return 1;
}

void PitchSamplingParameters::get_note_set(
  const AudioScaleSystem* scale_sys, int nsi, float* sts, int* num_sts) const {
  //
  (void) pss::get_note_set(scale_sys, sts, num_sts, nsi);
}

void PitchSamplingParameters::set_primary_note_set_index(
  PitchSamplingSystem* sys, AudioScaleSystem* scale_sys, int nsi) {
  //
  if (nsi != primary_note_set_index) {
    primary_note_set_index = set_note_set(sys, scale_sys, primary_pitch_sample_group_index, nsi);
  }
}

void PitchSamplingParameters::set_secondary_note_set_index(
  PitchSamplingSystem* sys, AudioScaleSystem* scale_sys, int nsi) {
  //
  if (nsi != secondary_note_set_index) {
    secondary_note_set_index = set_note_set(sys, scale_sys, secondary_pitch_sample_group_index, nsi);
  }
}

void PitchSamplingParameters::refresh_note_set_indices(
  PitchSamplingSystem* sys, AudioScaleSystem* scale_sys) {
  //
  primary_note_set_index = set_note_set(
    sys, scale_sys, primary_pitch_sample_group_index, primary_note_set_index);
  secondary_note_set_index = set_note_set(
    sys, scale_sys, secondary_pitch_sample_group_index, secondary_note_set_index);
}

void PitchSamplingParameters::set_ith_note_set_index(
  PitchSamplingSystem* sys, AudioScaleSystem* scale_sys, int group, int nsi) {
  //
  if (group == 0) {
    set_primary_note_set_index(sys, scale_sys, nsi);
  } else {
    assert(group == 1);
    set_secondary_note_set_index(sys, scale_sys, nsi);
  }
}

}