#pragma once

#include "grove/audio/audio_node.hpp"
#include "grove/audio/envelope.hpp"
#include "grove/audio/oscillator.hpp"
#include "grove/audio/audio_buffer.hpp"
#include "Reverb1Node.hpp"
#include "NoteSetNode.hpp"
#include "Granulator.hpp"
#include <atomic>

namespace grove {

struct AudioParameterSystem;
class AudioBufferStore;

class RandomizedSynth : public AudioProcessorNode {
public:
  struct Params {
    bool emit_events{true};
    bool use_oscillator{true};
    float reverb_mix_fraction{0.5f};
    float reverb_feedback_fraction{0.5f};
  };
public:
  RandomizedSynth(AudioParameterID node_id,
                  const AudioParameterSystem* parameter_system,
                  const AudioBufferStore* buffer_store,
                  AudioBufferHandle buffer_handle,
                  const Params& params);
  ~RandomizedSynth() override = default;

  GROVE_DECLARE_AUDIO_NODE_INTERFACE()
  GROVE_DECLARE_AUDIO_NODE_PARAMETER_DESCRIPTORS()

  void ui_randomize_note();
  bool render_should_randomize_note();

private:
  AudioParameterID node_id;

  InputAudioPorts input_ports;
  OutputAudioPorts output_ports;

  const AudioParameterSystem* parameter_system;

  const AudioBufferStore* buffer_store;
  AudioBufferHandle buffer_handle;

  Params params;
  AudioParameter<int, StaticLimits01<int>> use_oscillator{1};
  AudioParameter<float, StaticLimits01<float>> envelope_representation{0.0f};
  AudioParameter<int, StaticIntLimits<0, 127>> new_note_number_representation{0};

  env::ADSRExp<float> envelope;
  osc::WaveTable oscillator{};
  float global_gain{1.0f};

  Reverb1Node reverb;
  NoteSetNode note_set;
  MIDINote current_note{MIDINote::A4};

  Granulator granulator;

  std::atomic<bool> should_randomize_note{};
};

}