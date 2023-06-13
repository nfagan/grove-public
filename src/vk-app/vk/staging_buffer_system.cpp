#include "staging_buffer_system.hpp"
#include "buffer.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

using namespace vk;

void StagingBufferSystem::terminate() {
  free_buffers.clear();
  pending_release.clear();
}

void StagingBufferSystem::begin_frame() {
  auto pend_it = pending_release.begin();
  while (pend_it != pending_release.end()) {
    if (pend_it->future->is_ready()) {
      free_buffers.push_back(std::move(pend_it->buffer));
      pend_it = pending_release.erase(pend_it);
    } else {
      ++pend_it;
    }
  }
}

Result<ManagedBuffer> StagingBufferSystem::acquire(Allocator* allocator, size_t size) {
  for (int i = 0; i < int(free_buffers.size()); i++) {
    auto& buff = free_buffers[i];
    if (buff.contents().size >= size) {
      auto buff_res = std::move(buff);
      free_buffers.erase(free_buffers.begin() + i);
      return buff_res;
    }
  }
  return create_staging_buffer(allocator, size);
}

void StagingBufferSystem::release_sync(ManagedBuffer&& buff) {
  free_buffers.push_back(std::move(buff));
}

void StagingBufferSystem::release_async(CommandProcessor::CommandFuture future,
                                        ManagedBuffer&& buff) {
  PendingRelease pend{};
  pend.future = std::move(future);
  pend.buffer = std::move(buff);
  pending_release.push_back(std::move(pend));
}

size_t StagingBufferSystem::num_buffers() const {
  return pending_release.size() + free_buffers.size();
}

size_t StagingBufferSystem::approx_num_bytes_used() const {
  size_t res{};
  for (auto& free : free_buffers) {
    res += free.get_allocation_size();
  }
  for (auto& pend : pending_release) {
    res += pend.buffer.get_allocation_size();
  }
  return res;
}

GROVE_NAMESPACE_END
