#pragma once

#include "../audio_node.hpp"
#include "../AudioRecorder.hpp"

namespace grove {

struct AudioParameterSystem;

class DestinationNode : public AudioProcessorNode {
public:
  GROVE_DECLARE_CONSTEXPR_FLOAT_LIMITS(GainLimits, 0.0f, 2.0f);
  static constexpr float default_gain = 0.25f;

  struct RecordInfo {
    AudioRecorder* recorder{};
    AudioRecordStreamHandle stream_handle{};
  };

private:
  friend class AudioGraph;

public:
  DestinationNode(AudioParameterID node_id,
                  const AudioParameterSystem* parameter_system, int num_channels);
  ~DestinationNode() override = default;

  InputAudioPorts inputs() const override;
  OutputAudioPorts outputs() const override;

  uint32_t get_id() const override {
    return node_id;
  }

  void set_output_sample_buffer(Sample* out);
  void set_node_id(AudioParameterID node_id);

  void process(const AudioProcessData& in,
               const AudioProcessData& out,
               AudioEvents* events,
               const AudioRenderInfo& info) override;

  GROVE_DECLARE_AUDIO_NODE_PARAMETER_DESCRIPTORS()

  bool set_record_info(const RecordInfo& info);

private:
  void maybe_record_data(const AudioProcessData& in,
                         const AudioRenderInfo& info);

private:
  AudioParameterID node_id;
  const AudioParameterSystem* parameter_system;

  InputAudioPorts input_ports;
  Sample* out_samples;

  RingBuffer<RecordInfo, 2> pending_record_info;
  RecordInfo active_record_info{};

  AudioParameter<float, GainLimits> gain{default_gain};
  AudioParameter<float, StaticLimits01<float>> signal_repr{0.0f};
};


}