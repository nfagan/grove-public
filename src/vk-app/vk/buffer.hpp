#pragma once

#include "grove/vk/vk.hpp"

namespace grove::vk {

struct Core;
class CommandProcessor;

Result<ManagedBuffer> create_device_local_buffer(Allocator* allocator,
                                                 std::size_t size,
                                                 VkBufferUsageFlags usage);
Result<ManagedBuffer> create_host_visible_buffer(Allocator* allocator,
                                                 std::size_t size,
                                                 VkBufferUsageFlags usage);
Result<ManagedBuffer> create_host_visible_host_coherent_buffer(Allocator* allocator,
                                                               std::size_t size,
                                                               VkBufferUsageFlags usage);
Result<ManagedBuffer> create_uniform_buffer(Allocator* allocator, std::size_t size);
Result<ManagedBuffer> create_storage_buffer(Allocator* allocator, std::size_t size);
Result<ManagedBuffer> create_device_local_storage_buffer(Allocator* allocator, std::size_t size);
Result<ManagedBuffer> create_staging_buffer(Allocator* allocator, std::size_t size);
Result<ManagedBuffer> create_host_visible_vertex_buffer(Allocator* allocator, size_t size);
Result<ManagedBuffer> create_device_local_vertex_buffer(Allocator* allocator, size_t size,
                                                        bool transfer_dst);
Result<ManagedBuffer> create_host_visible_index_buffer(Allocator* allocator, size_t size);
Result<ManagedBuffer> create_device_local_index_buffer(Allocator* allocator, size_t size,
                                                       bool transfer_dst);

Result<ManagedBuffer> create_dynamic_uniform_buffer(Allocator* allocator,
                                                    size_t min_align,
                                                    std::size_t desired_element_stride,
                                                    std::size_t num_elements,
                                                    std::size_t* actual_element_stride,
                                                    std::size_t* actual_buffer_size);

Result<ManagedBuffer> create_dynamic_storage_buffer(Allocator* allocator,
                                                    size_t min_align,
                                                    std::size_t desired_element_stride,
                                                    std::size_t num_elements,
                                                    std::size_t* actual_element_stride,
                                                    std::size_t* actual_buffer_size);

Result<ManagedBuffer> create_device_local_vertex_buffer_sync(Allocator* allocator,
                                                             size_t size,
                                                             const void* data,
                                                             const Core* core,
                                                             CommandProcessor* uploader);
Result<ManagedBuffer> create_device_local_index_buffer_sync(Allocator* allocator,
                                                            size_t size,
                                                            const void* data,
                                                            const Core* core,
                                                            CommandProcessor* uploader);

template <typename Element>
Result<ManagedBuffer> create_dynamic_uniform_buffer(Allocator* allocator,
                                                    const VkPhysicalDeviceProperties* props,
                                                    std::size_t num_elements,
                                                    std::size_t* actual_element_stride,
                                                    std::size_t* actual_buffer_size) {
  return create_dynamic_uniform_buffer(
    allocator,
    props->limits.minUniformBufferOffsetAlignment,
    sizeof(Element),
    num_elements,
    actual_element_stride,
    actual_buffer_size);
}

template <typename Element>
Result<ManagedBuffer> create_dynamic_storage_buffer(Allocator* allocator,
                                                    const VkPhysicalDeviceProperties* props,
                                                    std::size_t num_elements,
                                                    std::size_t* actual_element_stride,
                                                    std::size_t* actual_buffer_size) {
  return create_dynamic_storage_buffer(
    allocator,
    props->limits.minStorageBufferOffsetAlignment,
    sizeof(Element),
    num_elements,
    actual_element_stride,
    actual_buffer_size);
}

}