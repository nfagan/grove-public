#pragma once

#include "grove/audio/types.hpp"
#include "grove/audio/audio_parameters.hpp"
#include "grove/common/Optional.hpp"

namespace grove {

template <typename P>
inline Optional<int> check_apply_int_param(P& p, const AudioParameterChangeView& param_changes) {
  AudioParameterChange change{};
  if (param_changes.collapse_to_last_change(&change)) {
    p.apply(change);
    return Optional<int>(p.evaluate());
  }
  return NullOpt{};
}

template <typename P>
void check_apply_float_param(P& p, const AudioParameterChangeView& param_changes) {
  AudioParameterChange change{};
  if (param_changes.collapse_to_last_change(&change)) {
    p.apply(change);
  }
}

template <typename P>
inline bool check_immediate_apply_float_param(P& p, const AudioParameterChangeView& param_changes) {
  AudioParameterChange change{};
  if (param_changes.collapse_to_last_change(&change)) {
    p.apply(change);
    p.jump_to_target();
    return true;
  } else {
    return false;
  }
}

}