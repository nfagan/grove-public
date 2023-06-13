#pragma once

#include <cstddef>

namespace grove {

template <typename T>
class Optional;

class AudioMemoryArena {
public:
  struct Block {
    unsigned char* data;
    std::size_t size;
  };

public:
  AudioMemoryArena();
  ~AudioMemoryArena();

  AudioMemoryArena(const AudioMemoryArena& other) = delete;
  AudioMemoryArena& operator=(const AudioMemoryArena& other) = delete;

  Block allocate(std::size_t bytes);
  Optional<Block> try_allocate(std::size_t bytes);

private:
  unsigned char* ptr;
  std::size_t size;
};

}