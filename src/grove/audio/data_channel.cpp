#include "data_channel.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Optional.hpp"

GROVE_NAMESPACE_BEGIN

/*
 * AudioProcessBuffer
 */

Optional<AudioProcessBuffer> detail::try_allocate(AudioMemoryArena& arena, std::size_t size) {
  if (auto maybe_allocated = arena.try_allocate(size)) {
    auto& as_alloced = maybe_allocated.value();
    AudioProcessBuffer buff{as_alloced.data, as_alloced.size};
    return Optional<AudioProcessBuffer>(buff);

  } else {
    return NullOpt{};
  }
}

void AudioProcessBuffer::zero() const {
  static_assert(std::is_trivial<AudioProcessBuffer>::value,
    "Expected AudioProcessBuffer to be trivial");
  //
  std::memset(data, 0, size);
}

/*
 * BufferChannelDescriptor
 */

bool BufferChannelDescriptor::is_float() const {
  return type == BufferDataType::Float;
}

bool BufferChannelDescriptor::is_sample2() const {
  return type == BufferDataType::Sample2;
}

bool BufferChannelDescriptor::is_bool() const {
  return type == BufferDataType::Bool;
}

bool BufferChannelDescriptor::is_int() const {
  return type == BufferDataType::Int;
}

bool BufferChannelDescriptor::is_midi_message() const {
  return type == BufferDataType::MIDIMessage;
}

uint32_t BufferChannelDescriptor::size() const {
  return BufferChannelDescriptor::size_of(type);
}

uint32_t BufferChannelDescriptor::ceil_div(uint32_t a, uint32_t b) {
  return a / b + (a % b != 0);
}

uint32_t BufferChannelDescriptor::size_of(BufferDataType type) {
  switch (type) {
    case BufferDataType::Float:
      return sizeof(float);
    case BufferDataType::Sample2:
      return sizeof(Sample2);
    case BufferDataType::Bool:
      return sizeof(bool);
    case BufferDataType::Int:
      return sizeof(int);
    case BufferDataType::MIDIMessage:
      return sizeof(MIDIMessage);
    default:
      assert(false);
      return 0;
  }
}

uint32_t BufferChannelDescriptor::align_of(BufferDataType type) {
  switch (type) {
    case BufferDataType::Float:
      return alignof(float);
    case BufferDataType::Sample2:
      return alignof(Sample2);
    case BufferDataType::Bool:
      return alignof(bool);
    case BufferDataType::Int:
      return alignof(int);
    case BufferDataType::MIDIMessage:
      return alignof(MIDIMessage);
    default:
      assert(false);
      return 0;
  }
}

GROVE_NAMESPACE_END
