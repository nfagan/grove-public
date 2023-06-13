#include "AudioMemoryArena.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Optional.hpp"
#include <cassert>
#include <memory>

GROVE_NAMESPACE_BEGIN

AudioMemoryArena::AudioMemoryArena() :
  ptr(nullptr),
  size(0) {
  //
  static_assert(sizeof(unsigned char) == 1, "Expected 1 byte for unsigned char.");
}

AudioMemoryArena::~AudioMemoryArena() {
  std::free(ptr);
  ptr = nullptr;
  size = 0;
}

AudioMemoryArena::Block AudioMemoryArena::allocate(std::size_t bytes) {
  if (bytes > size) {
    std::free(ptr);
    ptr = (unsigned char*) std::malloc(bytes);
    size = bytes;

    assert(ptr);
  }

  return {ptr, size};
}

Optional<AudioMemoryArena::Block> AudioMemoryArena::try_allocate(std::size_t bytes) {
  if (bytes <= size) {
    return Optional<AudioMemoryArena::Block>(allocate(bytes));
  } else {
    return NullOpt{};
  }
}

GROVE_NAMESPACE_END
