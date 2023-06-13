#include "rhythm_parameters.hpp"
#include "grove/common/common.hpp"
#include "grove/math/util.hpp"
#include <cassert>

GROVE_NAMESPACE_BEGIN

void RhythmParameters::set_global_p_quantized(float p) {
  assert(p >= 0.0f && p <= 1.0f);
  global_p_quantized = clamp01(p);
}

GROVE_NAMESPACE_END