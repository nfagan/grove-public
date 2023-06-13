#pragma once

#include "data_channel.hpp"
#include "grove/common/identifier.hpp"
#include <cstdint>

namespace grove {

struct AudioRecordStreamResult;

namespace audio {
  enum class BufferBackingStoreType {
    InMemory,
    File
  };
}

struct AudioBufferDescriptor {
public:
  using Layout = BufferChannelSet<4>;
public:
  int64_t num_channels() const {
    return layout.num_channels();
  }

  uint64_t total_num_frames() const {
    return size == 0 ? 0 : size / layout.stride();
  }

  bool is_compatible_with(const AudioBufferDescriptor& other) const;
  bool is_compatible_with(const BufferChannelDescriptors& other) const;
  bool is_n_channel_float(int num_channels) const;

  template <int N>
  DynamicArray<BufferChannelDescriptor, N> find_first_n_of_type(BufferDataType type, int n = N) const;

  static AudioBufferDescriptor from_interleaved_float(double sample_rate,
                                                      int num_frames, int num_channels);

  static AudioBufferDescriptor from_audio_record_stream_result(const AudioRecordStreamResult& result);

public:
  Layout layout;
  std::size_t size{};
  double sample_rate{};
};

template <int N>
DynamicArray<BufferChannelDescriptor, N>
AudioBufferDescriptor::find_first_n_of_type(BufferDataType type, int n) const {
  DynamicArray<BufferChannelDescriptor, N> result;

  for (int i = 0; i < layout.num_channels(); i++) {
    auto descriptor = layout.channel_descriptor(i);
    if (descriptor.type == type && result.size() < n) {
      result.push_back(descriptor);
    }
  }

  if (result.size() == n) {
    return result;
  } else {
    return {};
  }
}

struct AudioBufferChunk {
public:
  uint64_t num_frames_in_source() const {
    return descriptor.total_num_frames();
  }

  uint64_t frame_end() const {
    return frame_offset + frame_size;
  }

  bool is_in_bounds(uint64_t i) const {
    return i >= frame_offset && i < frame_offset + frame_size;
  }

  bool empty() const {
    return frame_size == 0;
  }

  bool is_complete() const {
    return frame_offset == 0 && descriptor.layout.frame_bytes(frame_size) == descriptor.size;
  }

  BufferChannelDescriptor channel_descriptor(int index) const {
    return descriptor.layout.channel_descriptor(index);
  }

  template <typename T>
  void read(int descriptor_index, uint64_t frame_index, T* out) const {
    assert(frame_index >= frame_offset && frame_index < frame_end());
    auto descrip = descriptor.layout.channel_descriptor(descriptor_index);
    descrip.read(data, frame_index - frame_offset, out);
  }

  template <typename T>
  void read(const BufferChannelDescriptor& descrip, uint64_t frame_index, T* out) const {
    descrip.read(data, frame_index - frame_offset, out);
  }

public:
  AudioBufferDescriptor descriptor;

  uint64_t frame_offset{};
  uint64_t frame_size{};

  unsigned char* data{};
};

struct AudioBufferHandle {
public:
  bool is_valid() const {
    return id != 0;
  }

public:
  GROVE_INTEGER_IDENTIFIER_STD_HASH(Hash, AudioBufferHandle, id)
  GROVE_INTEGER_IDENTIFIER_EQUALITY(AudioBufferHandle, id)
  GROVE_INTEGER_IDENTIFIER_INEQUALITIES(AudioBufferHandle, id)

public:
  uint64_t id{};
  audio::BufferBackingStoreType backing_store_type{audio::BufferBackingStoreType::InMemory};
};

namespace util {

struct LinearInterpolationInfo {
  double frac;
  uint64_t i0;
  uint64_t i1;
};

inline LinearInterpolationInfo make_linear_interpolation_info(double frame_index, uint64_t num_frames) {
  if (num_frames > 0) {
    auto s0 = std::floor(frame_index);
    auto f0 = frame_index - s0;
    auto i0 = uint64_t(s0);
    auto i1 = std::min(i0 + 1, num_frames - 1);
    return {f0, i0, i1};
  } else {
    return {};
  }
}

inline double tick_interpolating_frame_index_forwards_loop(double frame_index, double src_sr,
                                                           double out_sr, double rate_multiplier,
                                                           uint64_t total_num_frames) {
  frame_index += frame_index_increment(src_sr, out_sr, rate_multiplier);

  const auto num_frames = double(total_num_frames);
  while (num_frames > 0 && frame_index >= num_frames) {
    frame_index -= num_frames;
  }

  return frame_index;
}

inline float tick_interpolated_float(const unsigned char* data,
                                     const BufferChannelDescriptor& channel_descriptor,
                                     const LinearInterpolationInfo& interpolation_info) {
  assert(channel_descriptor.is_float());

  float v0;
  float v1;

  channel_descriptor.read(data, interpolation_info.i0, &v0);
  channel_descriptor.read(data, interpolation_info.i1, &v1);

  return float((1.0 - interpolation_info.frac) * v0 + interpolation_info.frac * v1);
}

inline float tick_interpolated_float(const AudioBufferChunk& chunk,
                                     const BufferChannelDescriptor& channel_descriptor,
                                     const LinearInterpolationInfo& interpolation_info) {
  assert(channel_descriptor.is_float());

  float v0;
  float v1;

  chunk.read(channel_descriptor, interpolation_info.i0, &v0);
  chunk.read(channel_descriptor, interpolation_info.i1, &v1);

  return float((1.0 - interpolation_info.frac) * v0 + interpolation_info.frac * v1);
}

}

}