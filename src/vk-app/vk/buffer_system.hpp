#pragma once

#include "upload.hpp"
#include "common.hpp"
#include "grove/vk/vk.hpp"
#include <unordered_set>
#include <vector>

namespace grove::vk {

class BufferSystem {
public:
  class BufferHandle {
    friend class BufferSystem;
  private:
    BufferHandle(BufferSystem* sys, std::shared_ptr<ManagedBuffer> buff);

  public:
    BufferHandle() = default;
    BufferHandle(BufferHandle&& other) noexcept;
    GROVE_NONCOPYABLE(BufferHandle)
    BufferHandle& operator=(BufferHandle&& other) noexcept;
    ~BufferHandle();

    bool is_valid() const {
      return buffer && buffer->is_valid();
    }
    ManagedBuffer& get() {
      GROVE_ASSERT(is_valid());
      return *buffer;
    }
    const ManagedBuffer& get() const {
      GROVE_ASSERT(is_valid());
      return *buffer;
    }
    friend inline void swap(BufferHandle& a, BufferHandle& b) noexcept {
      using std::swap;
      swap(a.system, b.system);
      swap(a.buffer, b.buffer);
    }
  private:
    BufferSystem* system{};
    std::shared_ptr<ManagedBuffer> buffer;
  };

public:
  void terminate();
  void begin_frame(const RenderFrameInfo& info);
  BufferHandle emplace(ManagedBuffer&& buff);
  void destroy_buffer(BufferHandle&& handle);
  size_t num_buffers() const;
  size_t approx_num_bytes_used() const;

private:
  void destroy_buffer(std::shared_ptr<ManagedBuffer>&& buff);

private:
  struct PendingDestruction {
    uint64_t frame_id{};
    std::shared_ptr<ManagedBuffer> buffer;
  };

  RenderFrameInfo frame_info{};
  std::vector<PendingDestruction> pending_destruction;
  std::unordered_set<std::shared_ptr<ManagedBuffer>> buffers;
};

}