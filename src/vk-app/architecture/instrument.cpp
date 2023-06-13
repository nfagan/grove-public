#include "instrument.hpp"
#include "../audio_processors/OscSwell.hpp"
#include "../audio_observation/OscSwell.hpp"
#include "../audio_observation/AudioObservation.hpp"
#include "../audio_core/AudioConnectionManager.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace arch;

ArchInstrumentObservableChange make_change(AudioNodeStorage::NodeID id, float value) {
  ArchInstrumentObservableChange result;
  result.id = id;
  result.value = value;
  return result;
}

} //  anon

ArchInstrumentCreateNodeResult arch::create_osc_swell(ArchInstrument* instrument,
                                                      const ArchInstrumentContext& context,
                                                      const ArchInstrumentCreateNodeParams& params) {
  auto node_ctor = [scale = &context.scale](AudioNodeStorage::NodeID node_id) {
    return new OscSwell(node_id, scale, /*enable_events=*/true);
  };

  auto& node_storage = context.node_storage;
  auto& observation = context.observation;
  auto port_descriptors = make_port_descriptors_from_audio_node_ctor(node_ctor);
  AudioNodeStorage::NodeID node_id = node_storage.create_node(node_ctor, port_descriptors);

  auto observable = observe::OscSwell::make_node(
    [instrument, node_id](const AudioParameterDescriptor&, const UIAudioParameter& val) {
      instrument->changes.push_back(make_change(node_id, val.fractional_value()));
    });

  observation.parameter_monitor.add_node(node_id, std::move(observable));
  auto create_node_res = context.node_placement.create_node(
    node_id,
    node_storage.get_port_info_for_node(node_id).unwrap(),
    params.port_position,
    params.port_y_offset);

  ArchInstrumentCreateNodeResult result;
  result.id = node_id;
  for (auto& info : create_node_res) {
    result.pending_placement.push_back(info);
  }
  return result;
}

void arch::destroy_osc_swell(ArchInstrument*, AudioNodeStorage::NodeID id,
                             const ArchInstrumentContext& context) {
  context.observation.parameter_monitor.remove_node(id, context.ui_parameter_manager);
  context.node_placement.delete_node(id, context.port_renderer);
  context.connection_manager.maybe_delete_node(id);
}

ArrayView<const ArchInstrumentObservableChange>
arch::read_changes(const ArchInstrument* instrument) {
  return make_view(instrument->changes);
}

void arch::clear_changes(ArchInstrument* instr) {
  instr->changes.clear();
}

GROVE_NAMESPACE_END
