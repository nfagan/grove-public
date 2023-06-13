#include "buffer.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace vk;

Buffer make_buffer(VkBuffer handle) {
  Buffer result{};
  result.handle = handle;
  return result;
}

bool is_host_coherent(MemoryProperty::Flag flag) {
  return flag & MemoryProperty::HostCoherent;
}

bool is_host_visible(MemoryProperty::Flag flag) {
  return flag & MemoryProperty::HostVisible;
}

} //  anon

vk::ManagedBuffer::ManagedBuffer(Allocator* allocator,
                                 AllocationRecordHandle allocation,
                                 MemoryProperty::Flag memory_properties,
                                 unsigned char* maybe_mapped_ptr,
                                 Buffer buffer,
                                 size_t size) :
  allocator{allocator},
  allocation{allocation},
  memory_properties{memory_properties},
  mapped_ptr{maybe_mapped_ptr},
  buffer{buffer},
  size{size} {
  //
}

vk::ManagedBuffer::ManagedBuffer(ManagedBuffer&& other) noexcept {
  swap(*this, other);
}

ManagedBuffer& vk::ManagedBuffer::operator=(ManagedBuffer&& other) noexcept {
  ManagedBuffer tmp{std::move(other)};
  swap(*this, tmp);
  return *this;
}

vk::ManagedBuffer::~ManagedBuffer() {
  if (allocator) {
    destroy();
  } else {
    GROVE_ASSERT(buffer.handle == VK_NULL_HANDLE && size == 0);
  }
}

vk::ManagedBuffer::Contents vk::ManagedBuffer::contents() const {
  return {buffer, size};
}

size_t vk::ManagedBuffer::get_allocation_size() const {
  if (allocator && allocation != null_allocation_record_handle()) {
    return allocator->get_size(allocation);
  } else {
    GROVE_ASSERT(false);
    return 0;
  }
}

bool vk::ManagedBuffer::is_host_visible() const {
  return grove::is_host_visible(memory_properties);
}

void vk::ManagedBuffer::write(const void* data, size_t write_size, size_t offset) const {
  GROVE_ASSERT(is_valid() && grove::is_host_visible(memory_properties));
  unsigned char* write_to;
  const bool need_unmap = get_ptr(&write_to);
  memcpy(write_to + offset, data, write_size);
  if (!is_host_coherent(memory_properties)) {
    allocator->flush_memory_range(allocation, offset, write_size);
  }
  if (need_unmap) {
    allocator->unmap_memory(allocation);
  }
}

void vk::ManagedBuffer::read(void* into, size_t read_size, size_t offset) const {
  GROVE_ASSERT(is_valid() && grove::is_host_visible(memory_properties));
  unsigned char* read_from;
  const bool need_unmap = get_ptr(&read_from);
  if (!is_host_coherent(memory_properties)) {
    allocator->invalidate_memory_range(allocation, offset, read_size);
  }
  memcpy(into, read_from + offset, read_size);
  if (need_unmap) {
    allocator->unmap_memory(allocation);
  }
}

void vk::ManagedBuffer::destroy() {
  GROVE_ASSERT(allocator && is_valid());
  if (mapped_ptr) {
    allocator->unmap_memory(allocation);
    mapped_ptr = nullptr;
  }
  allocator->destroy_buffer(buffer.handle, allocation);
  allocator = nullptr;
  allocation = null_allocation_record_handle();
  memory_properties = 0;
  mapped_ptr = nullptr;
  buffer = {};
  size = 0;
}

bool vk::ManagedBuffer::is_valid() const {
  return buffer.handle != VK_NULL_HANDLE;
}

bool vk::ManagedBuffer::get_ptr(unsigned char** ptr) const {
  if (mapped_ptr) {
    *ptr = mapped_ptr;
    return false;
  } else {
    *ptr = static_cast<unsigned char*>(allocator->map_memory(allocation));
    return true;
  }
}

vk::ManagedBufferView::ManagedBufferView(VkDevice device, VkBufferView view,
                                         VkFormat format, VkDeviceSize range, VkDeviceSize offset) :
  device{device},
  view{view},
  format{format},
  size{range},
  offset{offset} {
  assert(view != VK_NULL_HANDLE);
}

bool vk::ManagedBufferView::is_valid() const {
  return view != VK_NULL_HANDLE;
}

void vk::ManagedBufferView::clear() {
  device = VK_NULL_HANDLE;
  view = VK_NULL_HANDLE;
  format = {};
  size = 0;
  offset = 0;
}

vk::ManagedBufferView::ManagedBufferView(ManagedBufferView&& other) noexcept :
  device{other.device},
  view{other.view},
  format{other.format},
  size{other.size},
  offset{other.offset} {
  //
  other.clear();
}

vk::ManagedBufferView& vk::ManagedBufferView::operator=(ManagedBufferView&& other) noexcept {
  ManagedBufferView::~ManagedBufferView();
  device = other.device;
  view = other.view;
  format = other.format;
  size = other.size;
  offset = other.offset;
  other.clear();
  return *this;
}

vk::ManagedBufferView::Contents vk::ManagedBufferView::contents() const {
  assert(is_valid());
  vk::ManagedBufferView::Contents result{};
  result.view = view;
  return result;
}

vk::ManagedBufferView::~ManagedBufferView() {
  if (device != VK_NULL_HANDLE) {
    assert(view != VK_NULL_HANDLE);
    vkDestroyBufferView(device, view, GROVE_VK_ALLOC);
  }
  clear();
}

VkBufferCreateInfo vk::make_buffer_create_info(VkDeviceSize size,
                                               VkBufferUsageFlags usage,
                                               VkBufferCreateFlags flags,
                                               VkSharingMode share_mode,
                                               uint32_t num_queue_families,
                                               const uint32_t* queue_families) {
  VkBufferCreateInfo result{};
  result.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  result.sharingMode = share_mode;
  result.size = size;
  result.usage = usage;
  result.flags = flags;
  result.queueFamilyIndexCount = num_queue_families;
  result.pQueueFamilyIndices = queue_families;
  return result;
}

vk::Result<vk::ManagedBuffer> vk::create_managed_buffer(Allocator* allocator,
                                                        const VkBufferCreateInfo* create_info,
                                                        const AllocationCreateInfo* alloc_create_info) {
  VkBuffer buff_handle{};
  AllocationRecordHandle alloc_handle{};
  AllocationInfo alloc_info{};
  auto err = allocator->create_buffer(
    create_info, alloc_create_info, &buff_handle, &alloc_handle, &alloc_info);
  if (err) {
    return error_cast<vk::ManagedBuffer>(err);
  }

  unsigned char* maybe_mapped_ptr{};
  if (is_host_visible(alloc_info.memory_properties)) {
    maybe_mapped_ptr = static_cast<unsigned char*>(allocator->map_memory(alloc_handle));
    GROVE_ASSERT(maybe_mapped_ptr);
  }

  return ManagedBuffer{
    allocator,
    alloc_handle,
    alloc_info.memory_properties,
    maybe_mapped_ptr,
    make_buffer(buff_handle),
    create_info->size
  };
}

VkBufferViewCreateInfo vk::make_buffer_view_create_info(VkBuffer buffer, VkFormat format,
                                                        VkDeviceSize size, VkDeviceSize offset,
                                                        VkBufferViewCreateFlags flags) {
  VkBufferViewCreateInfo result{};
  result.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
  result.flags = flags;
  result.buffer = buffer;
  result.format = format;
  result.offset = offset;
  result.range = size;
  return result;
}

Result<ManagedBufferView>
vk::create_managed_buffer_view(VkDevice device, const VkBufferViewCreateInfo* create_info) {
  VkBufferView view_handle{};
  VkResult view_res = vkCreateBufferView(device, create_info, GROVE_VK_ALLOC, &view_handle);
  if (view_res != VK_SUCCESS) {
    return {view_res, "Failed to create ManagedBufferView."};
  } else {
    return ManagedBufferView{
      device, view_handle, create_info->format, create_info->range, create_info->offset
    };
  }
}

GROVE_NAMESPACE_END
