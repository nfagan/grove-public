#pragma once

#include "types.hpp"

namespace grove {

enum class OctaveDivision : uint8_t {
  EqualTemperament = 0
};

struct Tuning {
  OctaveDivision octave_division;
  uint8_t semitones_per_octave;
  double reference_semitone;
  double reference_frequency;
  PitchClass reference_pitch_class;
  int8_t reference_octave;
};

inline bool operator==(const Tuning& a, const Tuning& b) {
  return a.octave_division == b.octave_division &&
         a.semitones_per_octave == b.semitones_per_octave &&
         a.reference_semitone == b.reference_semitone &&
         a.reference_frequency == b.reference_frequency &&
         a.reference_pitch_class == b.reference_pitch_class &&
         a.reference_octave == b.reference_octave;
}

inline bool operator!=(const Tuning& a, const Tuning& b) {
  return !(a == b);
}

constexpr Tuning default_tuning() {
  return {
    /* .octave_division =       */ OctaveDivision::EqualTemperament,
    /* .semitones_per_octave =  */ 12,
    /* .reference_semitone =    */ 69.0,
    /* .reference_frequency =   */ 440.0,
    /* .reference_pitch_class = */ PitchClass::A,
    /* .reference_octave =      */ 4
  };
}

void note_number_to_note_components(uint8_t note_number,
                                    uint8_t st_per_oct,
                                    uint8_t reference_st,
                                    PitchClass reference_pitch_class,
                                    int8_t reference_oct,
                                    PitchClass* out_pitch_class,
                                    int8_t* out_oct);

void note_number_to_note_components(uint8_t note_number,
                                    const Tuning& tuning,
                                    PitchClass* pc,
                                    int8_t* oct);

double note_to_semitone(PitchClass pitch_class,
                        int8_t octave,
                        uint8_t st_per_oct,
                        double reference_st,
                        PitchClass reference_pitch_class,
                        int8_t reference_oct);
double note_to_semitone(PitchClass pitch_class, int8_t octave, const Tuning& tuning);

inline double semitone_to_frequency_equal_temperament(double st,
                                                      uint8_t st_per_oct,
                                                      double reference_st,
                                                      double reference_freq) {
  return reference_freq * std::pow(2.0, (st - reference_st) / st_per_oct);
}

inline double semitone_to_frequency_equal_temperament(double st, const Tuning& tuning) {
  return semitone_to_frequency_equal_temperament(
    st,
    tuning.semitones_per_octave,
    tuning.reference_semitone,
    tuning.reference_frequency);
}

inline double semitone_to_rate_multiplier_equal_temperament(double st,
                                                            uint8_t st_per_oct,
                                                            double reference_st,
                                                            double reference_frequency) {
  return reference_frequency / frequency_a4() * std::pow(2.0, (st - reference_st) / st_per_oct);
}

inline double semitone_to_rate_multiplier_equal_temperament(double st, const Tuning& tuning) {
  return semitone_to_rate_multiplier_equal_temperament(
    st,
    tuning.semitones_per_octave,
    tuning.reference_semitone,
    tuning.reference_frequency);
}

inline double note_number_to_semitone(uint8_t note_number) {
  return double(note_number);
}

inline double note_number_to_frequency_equal_temperament(uint8_t note_number,
                                                         const Tuning& tuning) {
  return semitone_to_frequency_equal_temperament(note_number_to_semitone(note_number), tuning);
}

}