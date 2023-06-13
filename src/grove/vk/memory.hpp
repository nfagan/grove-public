#pragma once

#include "common.hpp"
#include "grove/common/identifier.hpp"
#include "grove/common/Optional.hpp"

namespace grove::vk {

struct Instance;
struct PhysicalDevice;
struct Device;

//  https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkMemoryPropertyFlagBits.html
struct MemoryProperty {
  using Flag = uint32_t;
  static constexpr Flag DeviceLocal = 1u;             //  most efficient for device access, belongs to heap with VK_MEMORY_HEAP_DEVICE_LOCAL_BIT set
  static constexpr Flag HostVisible = 1u << 1u;       //  can be mapped for host access
  static constexpr Flag HostCoherent = 1u << 2u;      //  flushing and invalidating mapped ranges unnecessary
  static constexpr Flag HostCached = 1u << 3u;        //  memory is cached on the host; may be faster for host to access than uncached, may not be coherent
  static constexpr Flag LazilyAllocated = 1u << 4u;   //  only device access allowed. backing memory may be provided lazily.
};

struct AllocationProperty {
  using Flag = uint32_t;
  static constexpr Flag Dedicated = 1u;
};

VkMemoryPropertyFlags to_vk_memory_property_flags(MemoryProperty::Flag memory_properties);

Optional<uint32_t> find_memory_type(const VkPhysicalDeviceMemoryProperties& props,
                                    uint32_t impl_memory_type_requirements,
                                    VkMemoryPropertyFlags app_required_properties);

struct AllocationCreateInfo {
  MemoryProperty::Flag required_memory_properties{};
  MemoryProperty::Flag preferred_memory_properties{};
  AllocationProperty::Flag allocation_properties{};
  Optional<uint32_t> memory_type_index{};
};

struct AllocationInfo {
  uint32_t memory_type_index;
  MemoryProperty::Flag memory_properties;
  size_t size;
};

struct AllocationRecordHandle {
  GROVE_INTEGER_IDENTIFIER_EQUALITY(AllocationRecordHandle, id)
  GROVE_INTEGER_IDENTIFIER_INEQUALITIES(AllocationRecordHandle, id)

  uint64_t id{};
};

constexpr AllocationRecordHandle null_allocation_record_handle() {
  return AllocationRecordHandle{0};
}

struct AllocatorImpl;

class Allocator {
public:
  Allocator();
  ~Allocator();
  GROVE_NONCOPYABLE(Allocator)
  GROVE_NONMOVEABLE(Allocator)

  void create(const Instance* instance, const PhysicalDevice* physical_device, const Device* device);
  void destroy();

  [[nodiscard]] Error create_buffer(const VkBufferCreateInfo* buffer_create_info,
                                    const AllocationCreateInfo* alloc_create_info,
                                    VkBuffer* out_buff,
                                    AllocationRecordHandle* out_handle,
                                    AllocationInfo* out_info = nullptr);
  void destroy_buffer(VkBuffer buffer, AllocationRecordHandle handle);

  [[nodiscard]] Error create_image(const VkImageCreateInfo* image_create_info,
                                   const AllocationCreateInfo* alloc_create_info,
                                   VkImage* out_img,
                                   AllocationRecordHandle* out_handle,
                                   AllocationInfo* out_info = nullptr);
  void destroy_image(VkImage image, AllocationRecordHandle handle);

  void* map_memory(AllocationRecordHandle handle);
  void unmap_memory(AllocationRecordHandle handle);
  void flush_memory_range(AllocationRecordHandle handle, size_t offset, size_t size);
  void invalidate_memory_range(AllocationRecordHandle handle, size_t offset, size_t size);
  size_t get_size(AllocationRecordHandle handle) const;

private:
  AllocatorImpl* impl{};
  const PhysicalDevice* physical_device{};
};

}