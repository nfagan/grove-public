#include "DelayNode.hpp"
#include "grove/common/common.hpp"
#include "grove/math/util.hpp"

GROVE_NAMESPACE_BEGIN

DelayNode::DelayNode(double delay_time, double mix) :
  delay_time(std::max(delay_time, 0.001)),
  mix(mix),
  wp(0) {
  //
  InputAudioPort input_port(BufferDataType::Float, this, 0);
  input_ports.push_back(input_port);

  OutputAudioPort output_port(BufferDataType::Float, this, 0);
  output_ports.push_back(output_port);

  make_buffer();
}

OutputAudioPorts DelayNode::outputs() const {
  return output_ports;
}

InputAudioPorts DelayNode::inputs() const {
  return input_ports;
}

void DelayNode::process(const AudioProcessData& in,
                        const AudioProcessData& out,
                        AudioEvents*,
                        const AudioRenderInfo& info) {

  assert(in.descriptors.size() == 1 && in.descriptors[0].is_float());
  assert(out.descriptors.size() == 1 && out.descriptors[0].is_float());

  if (sample_rate != info.sample_rate) {
    sample_rate = info.sample_rate;
    make_buffer();
    wp = 0;
  }

  const int num_delay_frames = std::max(1, int(delay_time * info.sample_rate));
  const int num_frames = info.num_frames;

  for (int i = 0; i < num_frames; i++) {
    const int rp = read_ptr(num_delay_frames);
    const Sample delayed = buffer[rp];

    float active;
    in.descriptors[0].read<float>(in.buffer.data, i, &active);

    float mixed = grove::lerp(float(mix), active, delayed);
    out.descriptors[0].write<float>(out.buffer.data, i, &mixed);

    buffer[wp] = active;
    wp = (wp + 1) % num_delay_frames;
  }
}

void DelayNode::make_buffer() {
  auto num_frames = std::max(1, int(delay_time * sample_rate));
  buffer = std::make_unique<Sample[]>(num_frames);
}

int DelayNode::read_ptr(int num_delay_frames) const {
  int rp = wp - num_delay_frames;

  if (rp < 0) {
    rp += num_delay_frames;
  }

  return rp;
}

GROVE_NAMESPACE_END
