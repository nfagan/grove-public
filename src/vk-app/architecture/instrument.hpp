#pragma once

#include "../audio_core/AudioNodeStorage.hpp"
#include "../audio_core/SimpleAudioNodePlacement.hpp"
#include "grove/common/ArrayView.hpp"

namespace grove {

class AudioObservation;
class AudioScale;
class AudioParameterManager;
class UIAudioParameterManager;
class AudioConnectionManager;

}

namespace grove::arch {

struct ArchInstrumentObservableChange {
  AudioNodeStorage::NodeID id;
  float value;
};

struct ArchInstrumentCreateNodeResult {
  AudioNodeStorage::NodeID id;
  DynamicArray<SimpleAudioNodePlacement::PortInfo, 8> pending_placement;
};

struct ArchInstrumentCreateNodeParams {
  Vec3f port_position;
  float port_y_offset;
};

struct ArchInstrumentContext {
  AudioNodeStorage& node_storage;
  AudioConnectionManager& connection_manager;
  AudioObservation& observation;
  const AudioScale& scale;
  UIAudioParameterManager& ui_parameter_manager;
  SimpleAudioNodePlacement& node_placement;
  SimpleShapeRenderer& port_renderer;
};

struct ArchInstrument {
  DynamicArray<ArchInstrumentObservableChange, 4> changes;
};

ArchInstrumentCreateNodeResult create_osc_swell(ArchInstrument* instrument,
                                                const ArchInstrumentContext& context,
                                                const ArchInstrumentCreateNodeParams& params);
void destroy_osc_swell(ArchInstrument* instrument,
                       AudioNodeStorage::NodeID id, const ArchInstrumentContext& context);

ArrayView<const ArchInstrumentObservableChange> read_changes(const ArchInstrument* instrument);
void clear_changes(ArchInstrument* instr);

}