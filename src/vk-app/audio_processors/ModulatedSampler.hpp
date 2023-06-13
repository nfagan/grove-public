#pragma once

#include "Reverb1Node.hpp"
#include "grove/audio/audio_node.hpp"
#include "grove/audio/envelope.hpp"
#include "grove/audio/audio_buffer.hpp"

namespace grove {

class Transport;
class AudioBufferStore;
struct AudioParameterSystem;

class ModulatedSampler : public AudioProcessorNode {
  GROVE_DECLARE_CONSTEXPR_FLOAT_LIMITS(PitchModulationDepthLimits, 0.0f, 0.05f);

public:
  ModulatedSampler(AudioParameterID node_id,
                   const AudioBufferStore* store,
                   AudioBufferHandle buffer_handle,
                   const AudioParameterSystem* parameter_manager,
                   const Transport* transport);

  ~ModulatedSampler() override = default;

  GROVE_DECLARE_AUDIO_NODE_INTERFACE()
  GROVE_DECLARE_AUDIO_NODE_PARAMETER_DESCRIPTORS()

private:
  AudioParameterID node_id;

  InputAudioPorts input_ports;
  OutputAudioPorts output_ports;

  const AudioBufferStore* buffer_store;
  AudioBufferHandle buffer_handle;
  const Transport* transport;

  double frame_index{};
  double rate_multiplier{1.0};
  env::ADSRExp<float> envelope;
  int notes_on{};
  uint64_t last_render_frame{};

  AudioParameter<float, PitchModulationDepthLimits> pitch_modulation_depth{0.01f};
  double center_rate_multiplier{1.0};
  osc::Sin rate_multiplier_lfo{default_sample_rate(), 5.01};

  AudioParameter<float, StaticLimits01<float>> delay_mix{0.0f};
  audio::InterpolatedDelayLine<Sample2> rhythmic_delay{int(default_sample_rate() * 0.5)};

  Reverb1Node reverb;
};

}