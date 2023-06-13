#pragma once

#include "common.hpp"
#include "memory.hpp"
#include <memory>

namespace grove::vk {

struct Buffer {
  VkBuffer handle{VK_NULL_HANDLE};

  friend inline bool operator==(const Buffer& a, const Buffer& b) {
    return a.handle == b.handle;
  }
  friend inline bool operator!=(const Buffer& a, const Buffer& b) {
    return !(a == b);
  }
};

class ManagedBuffer {
public:
  struct Contents {
    Buffer buffer{};
    size_t size{};
  };

  ManagedBuffer() = default;
  ManagedBuffer(Allocator* allocator,
                AllocationRecordHandle allocation,
                MemoryProperty::Flag memory_properties,
                unsigned char* maybe_mapped_ptr,
                Buffer buffer,
                size_t size);
  GROVE_NONCOPYABLE(ManagedBuffer)
  ManagedBuffer(ManagedBuffer&& other) noexcept;
  ManagedBuffer& operator=(ManagedBuffer&& other) noexcept;
  ~ManagedBuffer();

  Contents contents() const;
  size_t get_allocation_size() const;
  void read(void* into, size_t read_size, size_t offset = 0) const;
  void write(const void* data, size_t write_size, size_t offset = 0) const;
  bool is_host_visible() const;

  void destroy();
  bool is_valid() const;

  friend inline void swap(ManagedBuffer& a, ManagedBuffer& b) noexcept {
    using std::swap;
    swap(a.allocator, b.allocator);
    swap(a.allocation, b.allocation);
    swap(a.memory_properties, b.memory_properties);
    swap(a.mapped_ptr, b.mapped_ptr);
    swap(a.buffer, b.buffer);
    swap(a.size, b.size);
  }
  friend inline bool operator==(const ManagedBuffer& a, const ManagedBuffer& b) {
    return a.allocator == b.allocator &&
           a.allocation == b.allocation &&
           a.memory_properties == b.memory_properties &&
           a.mapped_ptr == b.mapped_ptr &&
           a.buffer == b.buffer &&
           a.size == b.size;
  }
  friend inline bool operator!=(const ManagedBuffer& a, const ManagedBuffer& b) {
    return !(a == b);
  }

private:
  bool get_ptr(unsigned char** ptr) const;

private:
  Allocator* allocator{};
  AllocationRecordHandle allocation{};
  MemoryProperty::Flag memory_properties{};
  unsigned char* mapped_ptr{};
  Buffer buffer{};
  size_t size{};
};

class ManagedBufferView {
public:
  struct Contents {
    VkBufferView view{VK_NULL_HANDLE};
  };

public:
  ManagedBufferView() = default;
  ManagedBufferView(VkDevice device, VkBufferView view, VkFormat format,
                    VkDeviceSize range, VkDeviceSize offset);
  GROVE_NONCOPYABLE(ManagedBufferView)
  ManagedBufferView(ManagedBufferView&& other) noexcept;
  ManagedBufferView& operator=(ManagedBufferView&& other) noexcept;
  ~ManagedBufferView();
  bool is_valid() const;
  Contents contents() const;

  friend inline void swap(ManagedBufferView& a, ManagedBufferView& b) noexcept {
    using std::swap;
    swap(a.device, b.device);
    swap(a.view, b.view);
    swap(a.format, b.format);
    swap(a.size, b.size);
    swap(a.offset, b.offset);
  }
private:
  void clear();

private:
  VkDevice device{VK_NULL_HANDLE};
  VkBufferView view{VK_NULL_HANDLE};
  VkFormat format{};
  size_t size{};
  size_t offset{};
};

VkBufferCreateInfo make_buffer_create_info(VkDeviceSize size,
                                           VkBufferUsageFlags usage,
                                           VkBufferCreateFlags flags = 0,
                                           VkSharingMode share_mode = VK_SHARING_MODE_EXCLUSIVE,
                                           uint32_t num_queue_families = 0,
                                           const uint32_t* queue_families = nullptr);

Result<ManagedBuffer> create_managed_buffer(Allocator* allocator,
                                            const VkBufferCreateInfo* create_info,
                                            const AllocationCreateInfo* alloc_info);

VkBufferViewCreateInfo make_buffer_view_create_info(VkBuffer buffer,
                                                    VkFormat format,
                                                    VkDeviceSize size,
                                                    VkDeviceSize offset = 0,
                                                    VkBufferViewCreateFlags flags = 0);

Result<ManagedBufferView> create_managed_buffer_view(VkDevice device,
                                                     const VkBufferViewCreateInfo* create_info);

}