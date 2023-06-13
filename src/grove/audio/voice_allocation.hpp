#pragma once

#include "grove/common/Optional.hpp"
#include <cstdint>
#include <array>

namespace grove::audio {

template <int N>
class VoiceAllocator {
  static_assert(N > 0, "Expected non-empty voice array.");

public:
  struct Voice {
    uint64_t frame_on{};
    uint8_t note{};
    bool active{};
  };

public:
  int note_on(uint64_t frame, uint8_t note);
  int note_on_reuse_active(uint64_t frame, uint8_t note);

  Optional<int> note_off(uint8_t note) const;
  void deallocate(int ind);
  bool is_active(int ind) const {
    assert(ind >= 0 && ind < N);
    return voices[ind].active;
  }

private:
  void activate_voice(int ind, uint64_t frame, uint8_t note) {
    assert(ind >= 0 && ind < N);
    voices[ind].frame_on = frame;
    voices[ind].note = note;
    voices[ind].active = true;
  }

private:
  std::array<Voice, N> voices{};
};

template <int N>
int VoiceAllocator<N>::note_on(uint64_t frame, uint8_t note) {
  int use_ind{-1};
  int min_ind{-1};
  uint64_t min_frame{};

  for (int i = 0; i < int(voices.size()); i++) {
    auto& voice = voices[i];
    if (!voice.active) {
      use_ind = i;
      break;
    } else if (min_ind < 0 || voice.frame_on < min_frame) {
      min_frame = voice.frame_on;
      min_ind = i;
    }
  }

  if (use_ind < 0) {
    assert(min_ind >= 0);
    use_ind = min_ind;
  }

  activate_voice(use_ind, frame, note);
  return use_ind;
}

template <int N>
int VoiceAllocator<N>::note_on_reuse_active(uint64_t frame, uint8_t note) {
  for (int i = 0; i < int(voices.size()); i++) {
    if (voices[i].note == note) {
      activate_voice(i, frame, note);
      return i;
    }
  }

  return note_on(frame, note);
}

template <int N>
Optional<int> VoiceAllocator<N>::note_off(uint8_t note) const {
  int min_ind{-1};
  uint64_t min_frame{};

  for (int i = 0; i < int(voices.size()); i++) {
    const auto& voice = voices[i];
    if (voice.active && voice.note == note && (min_ind == -1 || voice.frame_on < min_frame)) {
      min_ind = i;
      min_frame = voice.frame_on;
    }
  }

  if (min_ind == -1) {
    return NullOpt{};
  } else {
    return Optional<int>(min_ind);
  }
}

template <int N>
void VoiceAllocator<N>::deallocate(int ind) {
  assert(ind >= 0 && ind < int(voices.size()) && voices[ind].active);
  voices[ind].active = false;
}

}