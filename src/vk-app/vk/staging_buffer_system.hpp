#pragma once

#include "grove/vk/buffer.hpp"
#include "command_processor.hpp"

namespace grove::vk {

class StagingBufferSystem {
  struct PendingRelease {
    CommandProcessor::CommandFuture future;
    ManagedBuffer buffer;
  };

public:
  void begin_frame();
  void terminate();
  Result<ManagedBuffer> acquire(Allocator* allocator, size_t size);
  void release_sync(ManagedBuffer&& buff);
  void release_async(CommandProcessor::CommandFuture future, ManagedBuffer&& buff);
  size_t num_buffers() const;
  size_t approx_num_bytes_used() const;

private:
  std::vector<PendingRelease> pending_release;
  std::vector<ManagedBuffer> free_buffers;
};

}