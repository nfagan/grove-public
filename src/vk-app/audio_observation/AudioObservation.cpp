#include "AudioObservation.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

void AudioObservation::update(UIAudioParameterManager& parameter_manager,
                              const AudioNodeStorage& node_storage) {
  parameter_monitor.update(parameter_manager, node_storage);
}

GROVE_NAMESPACE_END