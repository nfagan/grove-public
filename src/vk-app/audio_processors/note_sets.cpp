#include "note_sets.hpp"
#include "grove/math/random.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

PitchClass notes::uniform_sample_minor_key1() {
  const auto ind = int(urand() * 3.0);
  switch (ind) {
    case 0:
      return PitchClass(2);
    case 1:
      return PitchClass(5);
    case 2:
      return PitchClass(10);
    default:
      return PitchClass(2);
  }
}

PitchClass notes::uniform_sample_minor_key2() {
  const auto ind = int(urand() * 5.0);
  switch (ind) {
    case 0:
      return PitchClass(0);
    case 1:
      return PitchClass(2);
    case 2:
      return PitchClass(5);
    case 3:
      return PitchClass(7);
    case 4:
      return PitchClass(9);
    default:
      return PitchClass(2);
  }
}

PitchClass notes::uniform_sample_lydian_e() {
  const auto ind = int(urand() * 7.0);
  switch (ind) {
    case 0:
      return PitchClass::E;
    case 1:
      return PitchClass::Fs;
    case 2:
      return PitchClass::Gs;
    case 3:
      return PitchClass::A;
    case 4:
      return PitchClass::B;
    case 5:
      return PitchClass::Cs;
    case 6:
      return PitchClass::Ds;
    default:
      return PitchClass::E;
  }
}

GROVE_NAMESPACE_END