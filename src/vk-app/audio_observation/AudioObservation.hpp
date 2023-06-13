#pragma once

#include "RandomizedInstrumentNodes.hpp"
#include "ModulatedDelay1.hpp"
#include "RandomizedSynths.hpp"
#include "AudioParameterMonitor.hpp"

namespace grove {

class AudioObservation {
public:
  void update(UIAudioParameterManager& parameter_manager,
              const AudioNodeStorage& node_storage);

public:
  observe::AudioParameterMonitor parameter_monitor;
};

}