#pragma once

#include "grove/audio/AudioGraphRenderData.hpp"
#include "grove/audio/AudioGraphProxy.hpp"
#include "grove/audio/AudioGraphRenderer.hpp"
#include "grove/audio/AudioCore.hpp"

namespace grove {

class AudioGraphComponent {
  struct InitResult {
    DynamicArray<AudioRenderer::Modification, 4> render_modifications;
  };

public:
  InitResult initialize();
  void update(int frames_per_buffer);

private:
  AudioGraph graph;
  AudioGraphDoubleBuffer double_buffer;

public:
  AudioGraphProxy graph_proxy;
  AudioGraphRenderer renderer{&double_buffer};
};

}