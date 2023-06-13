#include "note_sets.hpp"
#include "grove/audio/types.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

int notes::ui_get_note_set0(float* offsets) {
  int num_offsets{};
  offsets[num_offsets++] = 0.0f;
  offsets[num_offsets++] = -12.0f;
  offsets[num_offsets++] = 12.0f;
  return num_offsets;
}

int notes::ui_get_note_set1(float* offsets) {
  //  2, 5, 7, 9, -12+2, -12+5, -12+7, -12+9
  int num_offsets{};
  offsets[num_offsets++] = 0.0f;
  offsets[num_offsets++] = 2.0f;
  offsets[num_offsets++] = 5.0f;
  offsets[num_offsets++] = 7.0f;
  offsets[num_offsets++] = 9.0f;
  offsets[num_offsets++] = -10.0f;
  offsets[num_offsets++] = -7.0f;
  offsets[num_offsets++] = -5.0f;
  offsets[num_offsets++] = -3.0f;
  return num_offsets;
}

int notes::ui_get_note_set2(float* offsets) {
  int num_offsets{};

  offsets[num_offsets++] = 0.0;
  offsets[num_offsets++] = 2.0;
  offsets[num_offsets++] = 5.0;
  offsets[num_offsets++] = 7.0;
  offsets[num_offsets++] = 9.0;

  offsets[num_offsets++] = 0.0 - 12.0;
  offsets[num_offsets++] = 2.0 - 12.0;
  offsets[num_offsets++] = 5.0 - 12.0;
  offsets[num_offsets++] = 7.0 - 12.0;
  offsets[num_offsets++] = 9.0 - 12.0;

  offsets[num_offsets++] = 0.0 + 12.0;
  offsets[num_offsets++] = 2.0 + 12.0;
  offsets[num_offsets++] = 5.0 + 12.0;
  offsets[num_offsets++] = 7.0 + 12.0;
  offsets[num_offsets++] = 9.0 + 12.0;

  return num_offsets;
}

int notes::ui_get_note_set3(float* offsets) {
  int num_offsets{};

  offsets[num_offsets++] = float(PitchClass::E);
  offsets[num_offsets++] = float(PitchClass::Fs);
  offsets[num_offsets++] = float(PitchClass::Gs);
  offsets[num_offsets++] = float(PitchClass::A);
  offsets[num_offsets++] = float(PitchClass::B);
  offsets[num_offsets++] = float(PitchClass::Cs);
  offsets[num_offsets++] = float(PitchClass::Ds);

  const int n = num_offsets;
  for (int i = 0; i < n; i++) {
    offsets[num_offsets++] = offsets[i] - 12.0f;
  }
  for (int i = 0; i < n; i++) {
    offsets[num_offsets++] = offsets[i] + 12.0f;
  }

  return num_offsets;
}

int notes::ui_get_pentatonic_major_note_set(float* offsets) {
  int num_offsets{};

  offsets[num_offsets++] = float(PitchClass::C);
  offsets[num_offsets++] = float(PitchClass::D);
  offsets[num_offsets++] = float(PitchClass::E);
  offsets[num_offsets++] = float(PitchClass::G);
  offsets[num_offsets++] = float(PitchClass::A);

  const int n = num_offsets;
  for (int i = 0; i < n; i++) {
    offsets[num_offsets++] = offsets[i] - 12.0f;
  }
  for (int i = 0; i < n; i++) {
    offsets[num_offsets++] = offsets[i] + 12.0f;
  }

  return num_offsets;
}

GROVE_NAMESPACE_END
