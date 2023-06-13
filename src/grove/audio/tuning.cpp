#include "tuning.hpp"
#include "grove/common/common.hpp"
#include "grove/math/util.hpp"

GROVE_NAMESPACE_BEGIN

void note_number_to_note_components(uint8_t note_number,
                                    uint8_t st_per_oct,
                                    uint8_t reference_st,
                                    PitchClass reference_pitch_class,
                                    int8_t reference_oct,
                                    PitchClass* out_pitch_class,
                                    int8_t* out_oct) {
  int ref_pc = int(reference_pitch_class) % st_per_oct;
  int delta_st = int(note_number) - int(reference_st);
  auto pc_off = ref_pc + delta_st;
  auto pc_wrapped = wrap_within_range(pc_off, int(st_per_oct));
  auto octave_offset = pc_off / int(st_per_oct);
  auto oct_off = pc_off < 0 && (std::abs(pc_off) % st_per_oct) != 0 ? -1 : 0;
  *out_pitch_class = PitchClass(pc_wrapped % 12);
  *out_oct = int8_t(octave_offset + int(reference_oct) + oct_off);
}

void note_number_to_note_components(uint8_t note_number,
                                    const Tuning& tuning,
                                    PitchClass* pc,
                                    int8_t* oct) {
  return note_number_to_note_components(
    note_number,
    tuning.semitones_per_octave,
    uint8_t(clamp(tuning.reference_semitone, 0.0, 255.0)),
    tuning.reference_pitch_class,
    tuning.reference_octave,
    pc,
    oct);
}

double note_to_semitone(PitchClass pitch_class,
                        int8_t octave,
                        uint8_t st_per_oct,
                        double reference_st,
                        PitchClass reference_pitch_class,
                        int8_t reference_oct) {
  auto pc_delta = int(pitch_class) - int(reference_pitch_class);
  auto pc_delta_st = double(pc_delta);
  int octave_delta = int(octave) - int(reference_oct);
  double octave_delta_st = double(octave_delta) * double(st_per_oct);
  return reference_st + pc_delta_st + octave_delta_st;
}

double note_to_semitone(PitchClass pitch_class, int8_t octave, const Tuning& tuning) {
  return note_to_semitone(
    pitch_class,
    octave,
    tuning.semitones_per_octave,
    tuning.reference_semitone,
    tuning.reference_pitch_class,
    tuning.reference_octave);
}

GROVE_NAMESPACE_END