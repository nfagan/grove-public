#pragma once

#include "../audio_node.hpp"
#include <cstring>

namespace grove {

template <typename T>
class MultiplyNode : public AudioProcessorNode {
public:
  explicit MultiplyNode();
  ~MultiplyNode() override = default;

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
MultiplyNode<T>::MultiplyNode() {
  for (int i = 0; i < 2; i++) {
    InputAudioPort input_port(WhichBufferDataType<T>::type, this, i);
    input_ports.push_back(input_port);
  }

  OutputAudioPort output_port(WhichBufferDataType<T>::type, this, 0);
  output_ports.push_back(output_port);
}

template <typename T>
InputAudioPorts MultiplyNode<T>::inputs() const {
  return input_ports;
}

template <typename T>
OutputAudioPorts MultiplyNode<T>::outputs() const {
  return output_ports;
}

template <typename T>
void MultiplyNode<T>::process(const AudioProcessData& in,
                              const AudioProcessData& out,
                              AudioEvents*,
                              const AudioRenderInfo& info) {

  GROVE_ASSERT_MATCHES_PORT_LAYOUTS(in, input_ports, out, output_ports);
  assert(out.descriptors.size() == 1 && in.descriptors.size() == 2);
  assert(out.descriptors[0].type == WhichBufferDataType<T>::type);

  for (int i = 0; i < info.num_frames; i++) {
    T a;
    in.descriptors[0].read<T>(in.buffer.data, i, &a);

    T b;
    in.descriptors[1].read<T>(in.buffer.data, i, &b);

    T c = a * b;
    out.descriptors[0].write<T>(out.buffer.data, i, &c);
  }
}

}