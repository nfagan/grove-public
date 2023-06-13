#pragma once

#include "grove/common/Optional.hpp"
#include <cstdint>

namespace grove {
class AudioComponent;
class Terrain;
class SimpleAudioNodePlacement;
class AudioPortPlacement;
}

namespace grove::pss {
struct PitchSamplingParameters;
}

namespace grove::tree {

struct RootsNewBranchInfo;

struct RootsInstrumentContext {
  AudioComponent& audio_component;
  SimpleAudioNodePlacement& node_placement;
  AudioPortPlacement& port_placement;
  const pss::PitchSamplingParameters& pitch_sampling_params;
  const Terrain& terrain;
};

struct RootsInstrumentUpdateResult {
  Optional<float> new_spectral_fraction;
};

RootsInstrumentUpdateResult
update_roots_spectrum_growth_instrument(const RootsInstrumentContext& context);

void update_roots_branch_spawn_instrument(
  const RootsInstrumentContext& context, const RootsNewBranchInfo* infos, int num_infos);

}