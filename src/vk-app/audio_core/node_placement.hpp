#pragma once

#include "SimpleAudioNodePlacement.hpp"
#include "audio_port_placement.hpp"
#include "AudioNodeStorage.hpp"

namespace grove {

class Terrain;

struct PlaceAudioNodeInWorldParams {
  const Terrain* terrain;
  float y_offset;
  SimpleAudioNodePlacement::NodeOrientation orientation;
};

void place_audio_node_in_world(
  AudioNodeStorage::NodeID node, const Vec3f& pos, const AudioNodeStorage& node_storage,
  AudioPortPlacement& port_placement, SimpleAudioNodePlacement& node_placement,
  const PlaceAudioNodeInWorldParams& params);

}