#include "memory.hpp"
#include "instance.hpp"
#include "device.hpp"
#include "physical_device.hpp"
#include "grove/common/common.hpp"
#include "grove/common/platform.hpp"
#include <unordered_map>

#ifdef GROVE_WIN
  #pragma warning( push, 1 )
#endif
#ifdef GROVE_MACOS
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wnullability-extension"
  #pragma clang diagnostic ignored "-Wnullability-completeness"
  #pragma clang diagnostic ignored "-Wunused-parameter"
  #pragma clang diagnostic ignored "-Wnested-anon-types"
  #pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
  #pragma clang diagnostic ignored "-Wmissing-field-initializers"
  #pragma clang diagnostic ignored "-Wunused-variable"
#endif

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#ifdef GROVE_WIN
  #pragma warning( pop )
#endif
#ifdef GROVE_MACOS
  #pragma clang diagnostic pop
#endif

GROVE_NAMESPACE_BEGIN

namespace vk {

namespace {

VmaAllocatorCreateInfo make_vma_allocator_create_info(VkInstance instance,
                                                      VkPhysicalDevice physical_device,
                                                      VkDevice device) {
  VmaAllocatorCreateInfo create_info{};
  create_info.flags = 0;
  create_info.preferredLargeHeapBlockSize = 0;
  create_info.pAllocationCallbacks = GROVE_VK_ALLOC;
  create_info.instance = instance;
  create_info.physicalDevice = physical_device;
  create_info.device = device;
  return create_info;
}

VmaAllocationCreateFlags to_vma_allocation_create_flags(AllocationProperty::Flag flags) {
  VmaAllocationCreateFlags res{};
  if (flags & AllocationProperty::Dedicated) {
    res |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
  }
  return res;
}

VmaAllocationCreateInfo to_vma_allocation_create_info(const AllocationCreateInfo& src) {
  VmaAllocationCreateInfo result{};
  result.usage = VMA_MEMORY_USAGE_UNKNOWN;
  result.requiredFlags = to_vk_memory_property_flags(src.required_memory_properties);
  result.preferredFlags = to_vk_memory_property_flags(src.preferred_memory_properties);
  result.flags = to_vma_allocation_create_flags(src.allocation_properties);
  if (src.memory_type_index) {
    result.memoryTypeBits = 1u << src.memory_type_index.value();
  }
  return result;
}

MemoryProperty::Flag to_grove_memory_property_flags(VkMemoryPropertyFlags flags) {
  MemoryProperty::Flag res{};
  if (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
    res |= MemoryProperty::DeviceLocal;
  }
  if (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
    res |= MemoryProperty::HostVisible;
  }
  if (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) {
    res |= MemoryProperty::HostCoherent;
  }
  if (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) {
    res |= MemoryProperty::HostCached;
  }
  if (flags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) {
    res |= MemoryProperty::LazilyAllocated;
  }
  return res;
}

AllocationInfo to_grove_allocation_info(const VmaAllocationInfo* info,
                                        const VkPhysicalDeviceMemoryProperties* mem_props) {
  GROVE_ASSERT(info->memoryType < mem_props->memoryTypeCount);
  AllocationInfo result{};
  result.size = info->size;
  result.memory_type_index = info->memoryType;
  result.memory_properties = to_grove_memory_property_flags(
    mem_props->memoryTypes[info->memoryType].propertyFlags);
  return result;
}

} //  anon

struct AllocationRecord {
  VmaAllocation allocation;
  AllocationInfo allocation_info;
};

struct AllocatorImpl {
  AllocationRecord* lookup(AllocationRecordHandle handle) {
    if (auto it = records.find(handle.id); it != records.end()) {
      return &it->second;
    } else {
      return nullptr;
    }
  }

  VmaAllocator allocator{VK_NULL_HANDLE};

  std::unordered_map<uint64_t, AllocationRecord> records;
  uint64_t next_record_handle_id{1};
};

Allocator::Allocator() : impl{new AllocatorImpl()} {
  //
}

Allocator::~Allocator() {
  delete impl;
}

size_t Allocator::get_size(AllocationRecordHandle handle) const {
  if (auto* record = impl->lookup(handle)) {
    return record->allocation_info.size;
  } else {
    GROVE_ASSERT(false);
    return 0;
  }
}

void Allocator::create(const Instance* instance,
                       const PhysicalDevice* phys_device,
                       const Device* device) {
  assert(impl->allocator == VK_NULL_HANDLE && !physical_device);
  auto create_info = make_vma_allocator_create_info(
    instance->handle, phys_device->handle, device->handle);

  VmaAllocator handle;
  auto res = vmaCreateAllocator(&create_info, &handle);
  (void) res;
  GROVE_ASSERT(res == VK_SUCCESS);
  impl->allocator = handle;
  physical_device = phys_device;
}

void Allocator::destroy() {
  vmaDestroyAllocator(impl->allocator);
  impl->allocator = VK_NULL_HANDLE;
  physical_device = nullptr;
}

Error Allocator::create_buffer(const VkBufferCreateInfo* buffer_create_info,
                               const AllocationCreateInfo* alloc_create_info,
                               VkBuffer* out_buff,
                               AllocationRecordHandle* out_handle,
                               AllocationInfo* out_info) {
#if 0
  VkBuffer buff_handle;
  auto buff_res = vkCreateBuffer(device.handle, create_info, GROVE_VK_ALLOC, &buff_handle);
  if (!buff_res) {
    return {buff_res, "Failed to create buffer."};
  }

  VkMemoryRequirements buffer_requirements{};
  vkGetBufferMemoryRequirements(device.handle, buff_handle, &buffer_requirements);

  auto& physical_mem_props = physical_device.info.memory_properties;
  const VkMemoryPropertyFlags required_mem_props{
    alloc_create_info->application_required_memory_properties
  };

  if (auto type_ind = find_memory_type(
    physical_mem_props, buffer_requirements.memoryTypeBits, required_mem_props)) {
    VkMemoryAllocateInfo alloc_info{};
  }
#else
  assert(alloc_create_info->required_memory_properties);
  const auto vma_alloc_create_info = to_vma_allocation_create_info(*alloc_create_info);

  VkBuffer buff_handle{};
  VmaAllocation alloc{};
  VmaAllocationInfo alloc_info{};
  auto res = vmaCreateBuffer(
    impl->allocator, buffer_create_info, &vma_alloc_create_info, &buff_handle, &alloc, &alloc_info);
  if (res != VK_SUCCESS) {
    return {res, "Failed to create buffer."};
  }

  AllocationRecordHandle record_handle{impl->next_record_handle_id++};
  GROVE_ASSERT(impl->records.count(record_handle.id) == 0);

  auto grove_alloc_info = to_grove_allocation_info(
    &alloc_info, &physical_device->info.memory_properties);
  if (out_info) {
    *out_info = grove_alloc_info;
  }

  AllocationRecord record{};
  record.allocation = alloc;
  record.allocation_info = grove_alloc_info;
  impl->records[record_handle.id] = record;

  *out_buff = buff_handle;
  *out_handle = record_handle;
  return {};
#endif
}

void Allocator::destroy_buffer(VkBuffer buffer, AllocationRecordHandle handle) {
  if (auto* record = impl->lookup(handle)) {
    vmaDestroyBuffer(impl->allocator, buffer, record->allocation);
  } else {
    GROVE_ASSERT(false);
  }
}

Error Allocator::create_image(const VkImageCreateInfo* image_create_info,
                              const AllocationCreateInfo* alloc_create_info,
                              VkImage* out_image,
                              AllocationRecordHandle* out_handle,
                              AllocationInfo* out_info) {
  assert(alloc_create_info->required_memory_properties);
  const auto vma_alloc_create_info = to_vma_allocation_create_info(*alloc_create_info);

  VkImage image_handle{};
  VmaAllocation alloc{};
  VmaAllocationInfo alloc_info{};
  auto res = vmaCreateImage(
    impl->allocator, image_create_info, &vma_alloc_create_info, &image_handle, &alloc, &alloc_info);
  if (res != VK_SUCCESS) {
    return {res, "Failed to create image."};
  }

  AllocationRecordHandle record_handle{impl->next_record_handle_id++};
  GROVE_ASSERT(impl->records.count(record_handle.id) == 0);

  auto grove_alloc_info = to_grove_allocation_info(
    &alloc_info, &physical_device->info.memory_properties);
  if (out_info) {
    *out_info = grove_alloc_info;
  }

  AllocationRecord record{};
  record.allocation = alloc;
  record.allocation_info = grove_alloc_info;
  impl->records[record_handle.id] = record;

  *out_image = image_handle;
  *out_handle = record_handle;
  return {};
}

void Allocator::destroy_image(VkImage image, AllocationRecordHandle handle) {
  if (auto* record = impl->lookup(handle)) {
    vmaDestroyImage(impl->allocator, image, record->allocation);
  } else {
    GROVE_ASSERT(false);
  }
}

void* Allocator::map_memory(AllocationRecordHandle handle) {
  if (auto* record = impl->lookup(handle)) {
    void* data;
    vmaMapMemory(impl->allocator, record->allocation, &data);
    return data;
  } else {
    GROVE_ASSERT(false);
    return nullptr;
  }
}

void Allocator::unmap_memory(AllocationRecordHandle handle) {
  if (auto* record = impl->lookup(handle)) {
    vmaUnmapMemory(impl->allocator, record->allocation);
  } else {
    GROVE_ASSERT(false);
  }
}

void Allocator::flush_memory_range(AllocationRecordHandle handle, size_t offset, size_t size) {
  if (auto* record = impl->lookup(handle)) {
    vmaFlushAllocation(impl->allocator, record->allocation, offset, size);
  } else {
    GROVE_ASSERT(false);
  }
}

void Allocator::invalidate_memory_range(AllocationRecordHandle handle, size_t offset, size_t size) {
  if (auto* record = impl->lookup(handle)) {
    vmaInvalidateAllocation(impl->allocator, record->allocation, offset, size);
  } else {
    GROVE_ASSERT(false);
  }
}

Optional<uint32_t> find_memory_type(const VkPhysicalDeviceMemoryProperties& props,
                                    uint32_t impl_memory_type_requirements,
                                    VkMemoryPropertyFlags app_required_properties) {
  //  https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkPhysicalDeviceMemoryProperties.html
  for (uint32_t i = 0; i < props.memoryTypeCount; i++) {
    const auto& prop_flags = props.memoryTypes[i].propertyFlags;
    const bool is_required_mem_type = impl_memory_type_requirements & (1 << i);
    const bool has_required_props = (prop_flags & app_required_properties) == app_required_properties;
    if (is_required_mem_type && has_required_props) {
      return Optional<uint32_t>(i);
    }
  }
  return NullOpt{};
}

VkMemoryPropertyFlags to_vk_memory_property_flags(MemoryProperty::Flag flags) {
  VkMemoryPropertyFlags res{};
  if (flags & MemoryProperty::DeviceLocal) {
    res |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  }
  if (flags & MemoryProperty::HostVisible) {
    res |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
  }
  if (flags & MemoryProperty::HostCoherent) {
    res |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  }
  if (flags & MemoryProperty::HostCached) {
    res |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
  }
  if (flags & MemoryProperty::LazilyAllocated) {
    res |= VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
  }
  return res;
}

} //  vk

GROVE_NAMESPACE_END
