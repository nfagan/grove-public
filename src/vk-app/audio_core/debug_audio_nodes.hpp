#pragma once

namespace grove {

class SelectedInstrumentComponents;
class AudioComponent;

}

namespace grove::debug {

struct DebugAudioNodesContext {
  AudioComponent& audio_component;
  SelectedInstrumentComponents& selected;
};

void render_audio_nodes_gui(const DebugAudioNodesContext& context);

}