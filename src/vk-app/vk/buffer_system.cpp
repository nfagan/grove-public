#include "buffer_system.hpp"
#include "buffer.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

using namespace vk;

using BufferHandle = BufferSystem::BufferHandle;

BufferHandle::BufferHandle(BufferSystem* sys, std::shared_ptr<ManagedBuffer> buff) :
  system{sys},
  buffer{std::move(buff)} {
  //
}

BufferHandle::~BufferHandle() {
  if (system && buffer && buffer->is_valid()) {
    system->destroy_buffer(std::move(buffer));
    system = nullptr;
    buffer = nullptr;
  }
}

BufferHandle::BufferHandle(BufferHandle&& other) noexcept :
  system{other.system},
  buffer{std::move(other.buffer)} {
  other.system = nullptr;
}

BufferSystem::BufferHandle& BufferHandle::operator=(BufferHandle&& other) noexcept {
  BufferHandle tmp{std::move(other)};
  swap(*this, tmp);
  return *this;
}

void vk::BufferSystem::terminate() {
  for (auto& buff : buffers) {
    if (buff->is_valid()) {
      buff->destroy();
    }
  }
  buffers.clear();
}

void vk::BufferSystem::begin_frame(const RenderFrameInfo& info) {
  frame_info = info;
  auto del_it = pending_destruction.begin();
  while (del_it != pending_destruction.end()) {
    if (del_it->frame_id == info.finished_frame_id) {
      auto& pend_buff = del_it->buffer;
      if (pend_buff->is_valid()) {
        pend_buff->destroy();
      }
      const size_t num_erased = buffers.erase(del_it->buffer);
      GROVE_ASSERT(num_erased == 1);
      (void) num_erased;
      del_it = pending_destruction.erase(del_it);
    } else {
      GROVE_ASSERT(del_it->frame_id + info.frame_queue_depth > info.current_frame_id);
      ++del_it;
    }
  }
}

BufferHandle vk::BufferSystem::emplace(ManagedBuffer&& buff) {
  auto buffer = std::make_shared<ManagedBuffer>(std::move(buff));
  buffers.insert(buffer);
  BufferHandle result{this, std::move(buffer)};
  return result;
}

void vk::BufferSystem::destroy_buffer(BufferHandle&& handle) {
  destroy_buffer(std::move(handle.buffer));
  handle = {};
}

void vk::BufferSystem::destroy_buffer(std::shared_ptr<ManagedBuffer>&& buff) {
  GROVE_ASSERT(buff && buff->is_valid());
  PendingDestruction pend{};
  pend.buffer = std::move(buff);
  pend.frame_id = frame_info.current_frame_id;
  pending_destruction.push_back(std::move(pend));
}

size_t vk::BufferSystem::num_buffers() const {
  return buffers.size();
}

size_t vk::BufferSystem::approx_num_bytes_used() const {
  size_t res{};
  for (auto& buff : buffers) {
    if (buff->is_valid()) {
      res += buff->get_allocation_size();
    }
  }
  return res;
}

GROVE_NAMESPACE_END
