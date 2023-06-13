#include "ProceduralFlowerBenderInstrument.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

constexpr float min_signal_value_spawn() {
  return 0.01f;
}

} //  anon

using MonitorableNode = observe::AudioParameterMonitor::MonitorableNode;
using UpdateResult = ProceduralFlowerBenderInstrument::UpdateResult;

MonitorableNode ProceduralFlowerBenderInstrument::create_instance(AudioNodeStorage::NodeID id) {
  auto quant_cb = [this, id](const AudioParameterDescriptor&, const UIAudioParameter& param) {
    quantization_changes.push_back({id, param.fractional_value()});
  };
  auto signal_cb = [this, id](const AudioParameterDescriptor&, const UIAudioParameter& param) {
    signal_changes.push_back({id, param.fractional_value()});
  };

  auto node = observe::Bender::make_node(std::move(quant_cb), std::move(signal_cb));
  instances[id] = {};
  return node;
}

UpdateResult ProceduralFlowerBenderInstrument::update() {
  UpdateResult result{};
  for (auto& [_, instance] : instances) {
    instance.signal_value *= 0.5f;
  }
  for (auto& change : signal_changes) {
    if (auto inst_it = instances.find(change.id); inst_it != instances.end()) {
      inst_it->second.signal_value = change.value;
    }
  }
  for (auto& change : quantization_changes) {
    if (auto inst_it = instances.find(change.id); inst_it != instances.end()) {
      auto& instance = inst_it->second;
      if (instance.signal_value > min_signal_value_spawn()) {
        result.spawn_particle = true;
      }
    }
  }
  signal_changes.clear();
  quantization_changes.clear();
  return result;
}

GROVE_NAMESPACE_END
