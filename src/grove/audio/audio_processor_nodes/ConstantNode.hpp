#pragma once

#include "../audio_node.hpp"

namespace grove {

template <typename T>
class ConstantNode : public AudioProcessorNode {
public:
  explicit ConstantNode(int num_output_channels);
  ~ConstantNode() override = default;

  InputAudioPorts inputs() const override {
    return input_ports;
  }
  OutputAudioPorts outputs() const override {
    return output_ports;
  }

  void process(const AudioProcessData& in,
               const AudioProcessData& out,
               AudioEvents* events,
               const AudioRenderInfo& info) override;

public:
  T value{};

private:
  InputAudioPorts input_ports;
  OutputAudioPorts output_ports;
};

/*
 * impl
 */

template <typename T>
ConstantNode<T>::ConstantNode(int num_output_channels) {
  for (int i = 0; i < num_output_channels; i++) {
    OutputAudioPort output(WhichBufferDataType<T>::type, this, i);
    output_ports.push_back(output);
  }
}

template <typename T>
void ConstantNode<T>::process(const AudioProcessData& in,
                              const AudioProcessData& out,
                              AudioEvents*,
                              const AudioRenderInfo& info) {
  (void) in;
  assert(in.descriptors.empty());
  assert(out.descriptors.size() == output_ports.size());
  int num_descriptors = out.descriptors.size();

  for (int i = 0; i < info.num_frames; i++) {
    for (int j = 0; j < num_descriptors; j++) {
      assert(out.descriptors[j].type == WhichBufferDataType<T>::type);
      out.descriptors[j].write(out.buffer.data, i, &value);
    }
  }
}

using ConstantFloatNode = ConstantNode<float>;

}