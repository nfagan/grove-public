#include "audio_buffer.hpp"
#include "AudioRecorder.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

bool AudioBufferDescriptor::is_compatible_with(const AudioBufferDescriptor& other) const {
  auto num_chans = layout.num_channels();
  if (other.layout.num_channels() != num_chans) {
    return false;
  }

  for (int i = 0; i < num_chans; i++) {
    auto self_descriptor = layout.channel_descriptor(i);
    auto other_descriptor = other.layout.channel_descriptor(i);
    if (self_descriptor.type != other_descriptor.type) {
      return false;
    }
  }

  return true;
}

bool AudioBufferDescriptor::is_compatible_with(const BufferChannelDescriptors& other) const {
  if (other.size() != layout.num_channels()) {
    return false;
  }

  for (int i = 0; i < int(other.size()); i++) {
    auto self_descriptor = layout.channel_descriptor(i);
    auto other_descriptor = other[i];
    if (self_descriptor.type != other_descriptor.type) {
      return false;
    }
  }

  return true;
}

bool AudioBufferDescriptor::is_n_channel_float(int num_channels) const {
  if (layout.num_channels() != num_channels) {
    return false;
  }

  for (const auto& descr : layout.read_channels()) {
    if (descr.type != BufferDataType::Float) {
      return false;
    }
  }

  return true;
}

AudioBufferDescriptor
AudioBufferDescriptor::from_interleaved_float(double sample_rate,
                                              int num_frames, int num_channels) {
  AudioBufferDescriptor result{};

  for (int i = 0; i < num_channels; i++) {
    result.layout.add(BufferDataType::Float);
  }

  result.layout.finalize();
  result.size = result.layout.frame_bytes(num_frames);
  result.sample_rate = sample_rate;

  return result;
}

AudioBufferDescriptor
AudioBufferDescriptor::from_audio_record_stream_result(const AudioRecordStreamResult& stream_res) {
  AudioBufferDescriptor result{};
  result.sample_rate = stream_res.sample_rate;
  result.layout = stream_res.layout;
  result.size = stream_res.size;
  return result;
}

GROVE_NAMESPACE_END