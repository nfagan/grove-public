#pragma once

namespace grove {

class AudioComponent;
class SimpleAudioNodePlacement;
class AudioPortPlacement;
class Terrain;
struct RhythmParameters;

}

namespace grove::pss {
struct PitchSamplingParameters;
}

namespace grove::weather {
struct Status;
}

namespace grove::env {

struct EnvironmentInstrumentUpdateInfo {
  AudioComponent& audio_component;
  SimpleAudioNodePlacement& node_placement;
  AudioPortPlacement& port_placement;
  const RhythmParameters& rhythm_params;
  const pss::PitchSamplingParameters& pitch_sample_params;
  const Terrain& terrain;
  double real_dt;
  const weather::Status& weather_status;
};

void update_environment_instruments(const EnvironmentInstrumentUpdateInfo& info);

}