#pragma once

#include "grove/audio/types.hpp"
#include "grove/common/DynamicArray.hpp"

namespace grove::notes {

inline PitchClass int_to_pitch_class(int v, int off) {
  return PitchClass(wrap_within_range(v + off, 12));
}

using DefaultPitchClasses = DynamicArray<PitchClass, 8>;
using DefaultOctaves = DynamicArray<int8_t, 8>;

template <typename Octaves = DefaultOctaves>
void center_biased_octave_set(Octaves& octaves) {
  octaves.push_back(int8_t(3));
  octaves.push_back(int8_t(3));
  octaves.push_back(int8_t(4));
  octaves.push_back(int8_t(5));
}

template <typename PitchClasses = DefaultPitchClasses>
void minor_key1(PitchClasses& pitch_classes, int off) {
  pitch_classes.push_back(int_to_pitch_class(2, off));
  pitch_classes.push_back(int_to_pitch_class(5, off));
  pitch_classes.push_back(int_to_pitch_class(10, off));
}

template <typename PitchClasses = DefaultPitchClasses>
void lydian_e(PitchClasses& pitch_classes, int off) {
  pitch_classes.push_back(int_to_pitch_class(int(PitchClass::E), off));
  pitch_classes.push_back(int_to_pitch_class(int(PitchClass::Fs), off));
  pitch_classes.push_back(int_to_pitch_class(int(PitchClass::Gs), off));
  pitch_classes.push_back(int_to_pitch_class(int(PitchClass::A), off));
  pitch_classes.push_back(int_to_pitch_class(int(PitchClass::B), off));
  pitch_classes.push_back(int_to_pitch_class(int(PitchClass::Cs), off));
  pitch_classes.push_back(int_to_pitch_class(int(PitchClass::Ds), off));
}

PitchClass uniform_sample_lydian_e();
PitchClass uniform_sample_minor_key1();
PitchClass uniform_sample_minor_key2();
}