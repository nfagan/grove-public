#include "SpectrumNode.hpp"
#include "grove/common/common.hpp"
#include "grove/audio/dft.hpp"
#include "grove/audio/fdft.hpp"
#include "grove/audio/AudioEventSystem.hpp"
#include "grove/audio/AudioRenderBufferSystem.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

void push_dft_buffer(float* dft_buff, uint32_t instance) {
  const audio_buffer_system::BufferChannelType chan_types[2]{
    audio_buffer_system::BufferChannelType::Float,
    audio_buffer_system::BufferChannelType::Float
  };

  audio_buffer_system::BufferView buff{};
  if (!audio_buffer_system::render_allocate(chan_types, 2, SpectrumNode::block_size, &buff)) {
    return;
  }

  memcpy(buff.data_ptr(), dft_buff, SpectrumNode::block_size * 2 * sizeof(float));

  auto stream = audio_event_system::default_event_stream();
  auto evt = make_new_render_buffer_audio_event();
  if (audio_event_system::render_push_event(stream, evt)) {
    audio_buffer_system::render_wait_for_event(evt.id, 1, instance, buff);
  } else {
    audio_buffer_system::render_free(buff);
  }
}

} //  anon

SpectrumNode::SpectrumNode(uint32_t node_id) : node_id{node_id} {
  input_ports.push_back(InputAudioPort{BufferDataType::Float, this, 0});
  output_ports.push_back(OutputAudioPort{BufferDataType::Float, this, 0});
}

void SpectrumNode::process(const AudioProcessData& in, const AudioProcessData& out, AudioEvents*,
                           const AudioRenderInfo& info) {
  GROVE_ASSERT_MATCHES_PORT_LAYOUTS(in, input_ports, out, output_ports);

  auto& in0 = in.descriptors[0];
  auto& out0 = out.descriptors[0];

  for (int i = 0; i < info.num_frames; i++) {
    float v;
    in0.read(in.buffer.data, i, &v);

    if (!between_blocks) {
      assert(dft_sample_index < block_size);
      samples[dft_sample_index++] = v;
      if (dft_sample_index == block_size) {
#if 0
        dft(samples.data(), dft_buff.data(), block_size);
#else
        fdft(dft_buff.data(), samples.data(), block_size);
#endif
        push_dft_buffer(dft_buff.data(), node_id);

        dft_sample_index = 0;
        between_blocks = true;
      }
    } else if (double(inter_block_index++) / info.sample_rate > refresh_interval_s) {
      inter_block_index = 0;
      between_blocks = false;
    }

    out0.write(out.buffer.data, i, &v);
  }
}

InputAudioPorts SpectrumNode::inputs() const {
  return input_ports;
}

OutputAudioPorts SpectrumNode::outputs() const {
  return output_ports;
}

GROVE_NAMESPACE_END
