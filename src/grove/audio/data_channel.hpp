#pragma once

#include "grove/common/DynamicArray.hpp"
#include "types.hpp"
#include "AudioMemoryArena.hpp"
#include <cstdint>
#include <cstring>

namespace grove {

struct AudioProcessBuffer {
public:
  void zero() const;

public:
  unsigned char* data;
  std::size_t size;
};

namespace detail {
  Optional<AudioProcessBuffer> try_allocate(AudioMemoryArena& arena, std::size_t size);
}

/*
 * BufferDataType
 */

enum class BufferDataType : uint8_t {
  Float = 0,
  Sample2,
  Bool,
  Int,
  MIDIMessage
};

template <typename T>
struct WhichBufferDataType {};

template <>
struct WhichBufferDataType<float> {
  static constexpr BufferDataType type = BufferDataType::Float;
};

template <>
struct WhichBufferDataType<Sample2> {
  static constexpr BufferDataType type = BufferDataType::Sample2;
};

template <>
struct WhichBufferDataType<bool> {
  static constexpr BufferDataType type = BufferDataType::Bool;
};

template <>
struct WhichBufferDataType<int> {
  static constexpr BufferDataType type = BufferDataType::Int;
};

template <>
struct WhichBufferDataType<MIDIMessage> {
  static_assert(std::is_trivial<MIDIMessage>::value, "Expected MIDIMessage to be trivial.");
  static_assert(std::is_standard_layout<MIDIMessage>::value,
    "Expected MIDIMessage to be of standard layout.");
  static constexpr BufferDataType type = BufferDataType::MIDIMessage;
};

template <typename T>
struct IsBufferDataType : public std::false_type {};

template <>
struct IsBufferDataType<float> : public std::true_type {};

template <>
struct IsBufferDataType<Sample2> : public std::true_type {};

template <>
struct IsBufferDataType<bool> : public std::true_type {};

template <>
struct IsBufferDataType<int> : public std::true_type {};

template <>
struct IsBufferDataType<MIDIMessage> : public std::true_type {};

/*
 * BufferChannelDescriptor
 */

class BufferChannelDescriptor {
private:
  template <int N>
  friend class BufferChannelSet;

public:
  uint64_t ptr_offset(int64_t index) const {
    return index * stride + offset;
  }

  template <typename T, typename Ptr>
  Ptr raw_ptr_at(Ptr ptr, int64_t index) const {
    static_assert(IsBufferDataType<T>::value, "Type is not a valid buffer data type.");
    return ptr + ptr_offset(index);
  }

  template <typename T>
  void read(const unsigned char* ptr, int64_t index, T* value) const {
    auto* src = raw_ptr_at<T, const unsigned char*>(ptr, index);
    std::memcpy(value, src, sizeof(T));
  }

  template <typename T>
  void write(unsigned char* ptr, int64_t index, const T* value) const {
    auto* dest = raw_ptr_at<T, unsigned char*>(ptr, index);
    std::memcpy(dest, value, sizeof(T));
  }

  bool is_missing() const {
    return stride == 0;
  }

  bool is_float() const;
  bool is_sample2() const;
  bool is_bool() const;
  bool is_int() const;
  bool is_midi_message() const;
  uint32_t size() const;

  static BufferChannelDescriptor missing() {
    BufferChannelDescriptor result{};
    return result;
  }

private:
  static uint32_t size_of(BufferDataType type);
  static uint32_t align_of(BufferDataType type);
  static uint32_t ceil_div(uint32_t a, uint32_t b);

public:
  BufferDataType type;
  uint32_t stride;
  int offset;
};

using BufferChannelDescriptors = DynamicArray<BufferChannelDescriptor, 8>;

/*
 * AudioProcessData
 */

struct AudioProcessData {
  AudioProcessBuffer buffer{};
  BufferChannelDescriptors descriptors;

  static inline AudioProcessData copy_excluding_descriptors(const AudioProcessData& src) {
    AudioProcessData res{};
    res.buffer = src.buffer;
    return res;
  }
};

/*
 * BufferChannelSet
 */

template <int N>
class BufferChannelSet {
private:
  struct Channel {
    BufferDataType type;
    int offset;
  };
  struct Descriptor {
    uint32_t size;
    uint32_t alignment;
  };

public:
  BufferChannelSet();

  int64_t add(BufferDataType channel_type);
  void finalize();

  BufferChannelDescriptor channel_descriptor(int index) const;
  AudioProcessBuffer allocate(AudioMemoryArena& arena, int count) const;
  void reserve(AudioMemoryArena& arena, int count) const;

  std::size_t frame_bytes(int64_t num_frames) const;

  int64_t num_channels() const;
  uint32_t stride() const;

  const DynamicArray<Channel, N>& read_channels() const {
    return channels;
  }

private:
  DynamicArray<Channel, N> channels;
  Descriptor descriptor;
};

/*
* Impl
*/

template <int N>
BufferChannelSet<N>::BufferChannelSet() :
  descriptor{} {
  //
}

template <int N>
int64_t BufferChannelSet<N>::add(BufferDataType channel_type) {
  auto id = channels.size();
  channels.push_back({channel_type, 0});
  return id;
}

template <int N>
int64_t BufferChannelSet<N>::num_channels() const {
  return channels.size();
}

template <int N>
BufferChannelDescriptor
BufferChannelSet<N>::channel_descriptor(int index) const {
  BufferChannelDescriptor desc{};
  auto& chan = channels[index];

  desc.type = chan.type;
  desc.stride = descriptor.size;
  desc.offset = chan.offset;

  return desc;
}

template <int N>
void BufferChannelSet<N>::finalize() {
  uint32_t off = 0;
  uint32_t max_align = 0;

  for (auto& channel : channels) {
    auto sz = BufferChannelDescriptor::size_of(channel.type);
    auto align = BufferChannelDescriptor::align_of(channel.type);

    auto next = BufferChannelDescriptor::ceil_div(off, align) * align;
    auto pad = next - off;

    off += pad;
    channel.offset = off;
    off += sz;

    max_align = std::max(max_align, align);
  }

  if (!channels.empty()) {
    auto next = BufferChannelDescriptor::ceil_div(off, max_align) * max_align;
    auto pad = next - off;

    descriptor.size = off + pad;
  }

  descriptor.alignment = max_align;
}

template <int N>
uint32_t BufferChannelSet<N>::stride() const {
  return descriptor.size;
}

template <int N>
AudioProcessBuffer BufferChannelSet<N>::allocate(AudioMemoryArena& arena, int count) const {
  auto size = count * descriptor.size;
  auto as_alloced = arena.allocate(size);
  return {as_alloced.data, as_alloced.size};
}

template <int N>
void BufferChannelSet<N>::reserve(AudioMemoryArena& arena, int count) const {
  (void) allocate(arena, count);
}

template <int N>
std::size_t BufferChannelSet<N>::frame_bytes(int64_t count) const {
  return std::size_t(count) * std::size_t{descriptor.size};
}

}