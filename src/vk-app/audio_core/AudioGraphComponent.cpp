#include "AudioGraphComponent.hpp"
#include "grove/audio/audio_config.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

AudioGraphComponent::InitResult AudioGraphComponent::initialize() {
  InitResult result;
  result.render_modifications.push_back(
    AudioCore::make_add_renderable_modification(&renderer));
  return result;
}

void AudioGraphComponent::update(int frames_per_buffer) {
  graph_proxy.update(graph, double_buffer, frames_per_buffer);
}

GROVE_NAMESPACE_END
