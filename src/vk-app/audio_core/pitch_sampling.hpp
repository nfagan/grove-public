#pragma once

#include <cstdint>

namespace grove {
struct PitchSamplingSystem;
struct PitchSampleSetGroupHandle;
struct AudioScaleSystem;
}

namespace grove::pss {

struct PitchSamplingParameters {
public:
  //  @TODO: Should be synced with note_sets.hpp
  static constexpr int max_num_notes = 32;

public:
  PitchSampleSetGroupHandle get_primary_group_handle(PitchSamplingSystem* sys) const;
  PitchSampleSetGroupHandle get_secondary_group_handle(PitchSamplingSystem* sys) const;
  int get_lydian_e_note_set_index();
  int get_minor_key1_note_set_index();
  int get_pentatonic_major_note_set_index();
  void get_note_set(const AudioScaleSystem* scale_sys, int nsi, float* sts, int* num_sts) const;

  void refresh_note_set_indices(PitchSamplingSystem* sys, AudioScaleSystem* scale_sys);
  void set_primary_note_set_index(PitchSamplingSystem* sys, AudioScaleSystem* scale_sys, int nsi);
  void set_secondary_note_set_index(PitchSamplingSystem* sys, AudioScaleSystem* scale_sys, int nsi);
  void set_ith_note_set_index(PitchSamplingSystem* sys, AudioScaleSystem* scale_sys, int group, int nsi);

public:
  uint32_t primary_pitch_sample_group_index{0};
  int primary_note_set_index{};

  uint32_t secondary_pitch_sample_group_index{1};
  int secondary_note_set_index{};

public:
  static int min_note_set_index();
  static int max_note_set_index();
};

}