#pragma once

#include "../audio_node.hpp"
#include <cstring>

namespace grove {

template <typename T>
class GainNode : public AudioProcessorNode {
public:
  explicit GainNode(int num_channels);
  ~GainNode() override = default;

  InputAudioPorts inputs() const override;
  OutputAudioPorts outputs() const override;

  void process(const AudioProcessData& in,
               const AudioProcessData& out,
               AudioEvents* events,
               const AudioRenderInfo& info) override;

private:
  InputAudioPorts input_ports;
  OutputAudioPorts output_ports;

public:
  T gain{};
};

/*
 * Impl
 */

template <typename T>
GainNode<T>::GainNode(int num_channels) {
  for (int i = 0; i < num_channels; i++) {
    InputAudioPort input_port(WhichBufferDataType<T>::type, this, i);
    OutputAudioPort output_port(WhichBufferDataType<T>::type, this, i);

    input_ports.push_back(input_port);
    output_ports.push_back(output_port);
  }
}

template <typename T>
InputAudioPorts GainNode<T>::inputs() const {
  return input_ports;
}

template <typename T>
OutputAudioPorts GainNode<T>::outputs() const {
  return output_ports;
}

template <typename T>
void GainNode<T>::process(const AudioProcessData& in,
                          const AudioProcessData& out,
                          AudioEvents*,
                          const AudioRenderInfo& info) {
  GROVE_ASSERT_MATCHES_PORT_LAYOUTS(in, input_ports, out, output_ports);
  assert(in.descriptors.size() == out.descriptors.size());

  const auto num_descriptors = in.descriptors.size();

  for (int i = 0; i < info.num_frames; i++) {
    for (int j = 0; j < num_descriptors; j++) {
      assert(in.descriptors[j].type == WhichBufferDataType<T>::type &&
             out.descriptors[j].type == WhichBufferDataType<T>::type);

      T value;
      in.descriptors[j].read<T>(in.buffer.data, i, &value);

      value *= gain;
      out.descriptors[j].write<T>(out.buffer.data, i, &value);
    }
  }
}

}