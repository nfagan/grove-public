#include "MergerNode.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

MergerNode::MergerNode() {
  input_ports.push_back(InputAudioPort(BufferDataType::Float, this, 0));
  input_ports.push_back(InputAudioPort(BufferDataType::Float, this, 1));

  output_ports.push_back(OutputAudioPort(BufferDataType::Sample2, this, 0));
}

InputAudioPorts MergerNode::inputs() const {
  return input_ports;
}

OutputAudioPorts MergerNode::outputs() const {
  return output_ports;
}

void MergerNode::process(const AudioProcessData& in,
                         const AudioProcessData& out,
                         AudioEvents*,
                         const AudioRenderInfo& info) {

  assert(in.descriptors.size() == 2 &&
         in.descriptors[0].is_float() && in.descriptors[1].is_float());
  assert(out.descriptors.size() == 1 && out.descriptors[0].is_sample2());

  for (int i = 0; i < info.num_frames; i++) {
    float chan0;
    float chan1;

    in.descriptors[0].read<float>(in.buffer.data, i, &chan0);
    in.descriptors[1].read<float>(in.buffer.data, i, &chan1);

    Sample2 samples;
    samples.samples[0] = chan0;
    samples.samples[1] = chan1;

    out.descriptors[0].write<Sample2>(out.buffer.data, i, &samples);
  }
}


GROVE_NAMESPACE_END
