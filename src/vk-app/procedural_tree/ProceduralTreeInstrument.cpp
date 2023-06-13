#include "ProceduralTreeInstrument.hpp"
#include "grove/common/common.hpp"
#include "../audio_core/UIAudioParameterManager.hpp"

GROVE_NAMESPACE_BEGIN

ProceduralTreeInstrument::Instance
ProceduralTreeInstrument::create_instance(AudioNodeStorage::NodeID id) {
  Instance instance{};
  instance.callback = [this, id](const AudioParameterDescriptor& desc, const UIAudioParameter& value) {
    changes.push_back({id, desc.ids.self, value.fractional_value()});
  };
  return instance;
}

ProceduralTreeInstrument::ObservableChanges ProceduralTreeInstrument::update() {
  auto res = std::move(changes);
  changes.clear();
  return res;
}

GROVE_NAMESPACE_END
