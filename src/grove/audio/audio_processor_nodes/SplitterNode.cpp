#include "SplitterNode.hpp"
#include "grove/common/common.hpp"
#include <cstring>

GROVE_NAMESPACE_BEGIN

SplitterNode::SplitterNode() {
  input_ports.push_back(InputAudioPort(BufferDataType::Sample2, this, 0));

  output_ports.push_back(OutputAudioPort(BufferDataType::Float, this, 0));
  output_ports.push_back(OutputAudioPort(BufferDataType::Float, this, 1));
}

InputAudioPorts SplitterNode::inputs() const {
  return input_ports;
}

OutputAudioPorts SplitterNode::outputs() const {
  return output_ports;
}

void SplitterNode::process(const AudioProcessData& in,
                           const AudioProcessData& out,
                           AudioEvents*,
                           const AudioRenderInfo& info) {
  assert(in.descriptors.size() == 1 && in.descriptors[0].is_sample2());
  assert(out.descriptors.size() == 2 &&
         out.descriptors[0].is_float() && out.descriptors[1].is_float());

  for (int i = 0; i < info.num_frames; i++) {
    Sample2 read_samples;
    in.descriptors[0].read<Sample2>(in.buffer.data, i, &read_samples);

    auto* chan0 = out.descriptors[0].raw_ptr_at<float>(out.buffer.data, i);
    auto* chan1 = out.descriptors[1].raw_ptr_at<float>(out.buffer.data, i);

    float s0 = read_samples.samples[0];
    float s1 = read_samples.samples[1];

    std::memcpy(chan0, &s0, sizeof(float));
    std::memcpy(chan1, &s1, sizeof(float));
  }
}

GROVE_NAMESPACE_END
