#pragma once

#include "SimpleAudioNodePlacement.hpp"

namespace grove {
class AudioNodeStorage;
class UIAudioParameterManager;
class Terrain;
struct AudioParameterSystem;
class KeyTrigger;
}

namespace grove::debug {

struct DebugAudioParameterEventsContext {
  AudioNodeStorage& node_storage;
  UIAudioParameterManager& ui_parameter_manager;
  SimpleAudioNodePlacement& node_placement;
  AudioParameterSystem* param_sys;
  const Terrain& terrain;
  const KeyTrigger& key_trigger;
};

SimpleAudioNodePlacement::CreateNodeResult
initialize_debug_audio_parameter_events(const DebugAudioParameterEventsContext& ctx);
void update_debug_audio_parameter_events(const DebugAudioParameterEventsContext& ctx);
void render_debug_audio_parameter_events_gui(const DebugAudioParameterEventsContext& ctx);

}