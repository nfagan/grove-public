#pragma once

#include "grove/audio/types.hpp"
#include "grove/math/util.hpp"

namespace grove::audio {

struct PitchCVMap {
  static constexpr double min_semitone = double(midi_note_number_c3()) - 5.0 * 12.0;    //  c -2
  static constexpr double max_semitone = min_semitone + 10.0 * 12.0;                    //  c +8
  static constexpr double semitone_span = max_semitone - min_semitone;

  static constexpr double min_cv = -1.0;
  static constexpr double max_cv = 1.0;
  static constexpr double cv_span = max_cv - min_cv;

  static inline double semitone_to_cv(double st) {
    st = clamp(st, PitchCVMap::min_semitone, PitchCVMap::max_semitone);
    const double st_frac = (st - PitchCVMap::min_semitone) / PitchCVMap::semitone_span;
    return st_frac * PitchCVMap::cv_span + PitchCVMap::min_cv;
  }
};

}