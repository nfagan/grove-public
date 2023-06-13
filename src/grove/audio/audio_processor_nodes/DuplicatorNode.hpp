#pragma once

#include "../audio_node.hpp"
#include <cstring>

namespace grove {

template <typename T>
class DuplicatorNode : public AudioProcessorNode {
public:
  explicit DuplicatorNode(int num_output_channels);
  ~DuplicatorNode() override = default;

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
DuplicatorNode<T>::DuplicatorNode(int num_output_channels) {
  for (int i = 0; i < num_output_channels; i++) {
    OutputAudioPort output_port(WhichBufferDataType<T>::type, this, i);
    output_ports.push_back(output_port);
  }

  InputAudioPort input_port(WhichBufferDataType<T>::type, this, 0);
  input_ports.push_back(input_port);
}

template <typename T>
InputAudioPorts DuplicatorNode<T>::inputs() const {
  return input_ports;
}

template <typename T>
OutputAudioPorts DuplicatorNode<T>::outputs() const {
  return output_ports;
}

template <typename T>
void DuplicatorNode<T>::process(const AudioProcessData& in,
                                const AudioProcessData& out,
                                AudioEvents*,
                                const AudioRenderInfo& info) {

  assert(in.descriptors.size() == input_ports.size() &&
         in.descriptors.size() == 1);
  assert(out.descriptors.size() == output_ports.size());

  const auto num_out_descriptors = out.descriptors.size();
  auto in_descriptor = in.descriptors[0];
  assert(in_descriptor.type == WhichBufferDataType<T>::type);

  for (int i = 0; i < info.num_frames; i++) {
    auto* read_p = in_descriptor.raw_ptr_at<T>(in.buffer.data, i);

    for (int j = 0; j < num_out_descriptors; j++) {
      assert(out.descriptors[j].type == WhichBufferDataType<T>::type);
      auto* write_p = out.descriptors[j].raw_ptr_at<T>(out.buffer.data, i);
      std::memcpy(write_p, read_p, sizeof(T));
    }
  }
}

}