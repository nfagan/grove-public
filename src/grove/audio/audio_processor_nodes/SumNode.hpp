#pragma once

#include "../audio_node.hpp"
#include <cstring>

namespace grove {

template <typename T>
class SumNode : public AudioProcessorNode {
public:
  explicit SumNode(int num_channels);
  ~SumNode() override = default;

  InputAudioPorts inputs() const override;
  OutputAudioPorts outputs() const override;

  void process(const AudioProcessData& in,
               const AudioProcessData& out,
               AudioEvents* events,
               const AudioRenderInfo& info) override;

private:
  InputAudioPorts input_ports;
  OutputAudioPorts output_ports;
};

/*
 * Impl
 */

template <typename T>
SumNode<T>::SumNode(int num_channels) {
  for (int i = 0; i < num_channels; i++) {
    input_ports.push_back(
      InputAudioPort{WhichBufferDataType<T>::type, this, i, AudioPort::Flags::marked_optional()});
  }

  OutputAudioPort output_port(WhichBufferDataType<T>::type, this, 0);
  output_ports.push_back(output_port);
}

template <typename T>
InputAudioPorts SumNode<T>::inputs() const {
  return input_ports;
}

template <typename T>
OutputAudioPorts SumNode<T>::outputs() const {
  return output_ports;
}

template <typename T>
void SumNode<T>::process(const AudioProcessData& in,
                         const AudioProcessData& out,
                         AudioEvents*,
                         const AudioRenderInfo& info) {

  assert(in.descriptors.size() == input_ports.size());
  assert(out.descriptors.size() == output_ports.size() &&
         out.descriptors.size() == 1);

  const auto num_in_descriptors = in.descriptors.size();
  auto out_descriptor = out.descriptors[0];

  assert(out_descriptor.type == WhichBufferDataType<T>::type);

  for (int i = 0; i < info.num_frames; i++) {
    T value{};

    for (int j = 0; j < num_in_descriptors; j++) {
      if (!in.descriptors[j].is_missing()) {
        assert(in.descriptors[j].type == WhichBufferDataType<T>::type);

        T plus;
        in.descriptors[j].read<T>(in.buffer.data, i, &plus);
        value += plus;
      }
    }

    out_descriptor.write<T>(out.buffer.data, i, &value);
  }
}

}