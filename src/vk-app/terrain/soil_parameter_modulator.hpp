#pragma once

#include "../audio_core/UIAudioParameterManager.hpp"
#include "../audio_core/AudioNodeStorage.hpp"
#include "grove/audio/AudioParameterWriteAccess.hpp"
#include "grove/math/vector.hpp"

namespace grove {

struct SoilGUIUpdateResult;
class Soil;
struct AudioParameterSystem;

}

namespace grove::soil {

struct ParameterModulator {
  AudioNodeStorage::NodeID target_node{};
  DynamicArray<AudioParameterDescriptor, 4> targets;
  Optional<AudioParameterWriterID> parameter_writer_id;
  bool enabled{};
  bool lock_targets{};
};

struct ParameterModulatorUpdateContext {
  UIAudioParameterManager& parameter_manager;
  AudioParameterSystem* parameter_system;
  const AudioNodeStorage& node_storage;
  Optional<AudioNodeStorage::NodeID> selected_node;
  const Vec3f& soil_quality;
};

void update_parameter_modulator(ParameterModulator& modulator,
                                const ParameterModulatorUpdateContext& context);
void on_gui_update(ParameterModulator& modulator, const SoilGUIUpdateResult& res);

}