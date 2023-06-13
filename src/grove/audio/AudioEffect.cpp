#include "AudioEffect.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

void AudioEffect::toggle_enabled() {
  if (is_enabled()) {
    disable();
  } else {
    enable();
  }
}

AudioParameterDescriptors AudioEffect::parameter_descriptors() const {
  return {};
}

AudioParameterID AudioEffect::parameter_parent_id() const {
  return null_audio_parameter_id();
}

GROVE_NAMESPACE_END
