#pragma once

#include "grove/common/Optional.hpp"
#include "grove/audio/tuning.hpp"
#include "grove/audio/AudioCore.hpp"
#include "grove/audio/audio_device.hpp"
#include "grove/common/Stopwatch.hpp"

namespace grove {

class AudioObservation;
class SimpleAudioNodePlacement;
class AudioComponent;
class AudioPortPlacement;

struct AudioGUIUpdateResult {
  Optional<Tuning> tuning;
  Optional<AudioDeviceInfo> change_device;
  Optional<AudioCore::FrameInfo> new_frame_info;
  Optional<bool> tuning_controlled_by_environment;
  Optional<bool> metronome_enabled;
  Optional<double> new_bpm;
  bool toggle_stream_started{};
  bool close{};
};

struct AudioGUIRenderParams {
  Optional<uint32_t> selected_node_id;
  const AudioObservation& observation;
  const AudioPortPlacement& port_placement;
  const SimpleAudioNodePlacement& node_placement;
  bool tuning_controlled_by_environment;
};

class AudioGUI {
public:
  AudioGUIUpdateResult render(const AudioComponent& component, const AudioGUIRenderParams& params);

public:
  Stopwatch stopwatch;
  float cpu_load{};
  Optional<uint32_t> selected_spectrum_node;
  std::vector<float> spectrum_data;
  bool show_spectrum{};
};

}