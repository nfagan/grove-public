#include "buffer.hpp"
#include "command_processor.hpp"
#include "grove/common/common.hpp"
#include "grove/common/memory.hpp"

GROVE_NAMESPACE_BEGIN

using namespace vk;

namespace {

Error copy_staging_buffer_sync(Allocator* allocator,
                               VkBuffer dst_buff,
                               const void* data,
                               size_t size,
                               const Core* core,
                               CommandProcessor* uploader) {
  auto stage_res = grove::create_staging_buffer(allocator, size);
  if (!stage_res) {
    return {stage_res.status, stage_res.message};
  } else {
    stage_res.value.write(data, size);
  }

  VkBufferCopy copy{};
  copy.size = size;
  auto src_buff = stage_res.value.contents().buffer.handle;
  auto transfer = [src_buff, dst_buff, copy](VkCommandBuffer cmd) {
    vkCmdCopyBuffer(cmd, src_buff, dst_buff, 1, &copy);
  };

  if (auto err = uploader->sync_graphics_queue(*core, std::move(transfer))) {
    return {err.result, err.message};
  }

  return {};
}

} //  anon

Result<ManagedBuffer> vk::create_device_local_buffer(Allocator* allocator,
                                                     std::size_t size,
                                                     VkBufferUsageFlags usage) {
  const auto buff_create_info = make_buffer_create_info(size, usage);
  AllocationCreateInfo alloc_info{};
  alloc_info.required_memory_properties = MemoryProperty::DeviceLocal;
  return create_managed_buffer(allocator, &buff_create_info, &alloc_info);
}

Result<ManagedBuffer> vk::create_host_visible_buffer(Allocator* allocator,
                                                     std::size_t size,
                                                     VkBufferUsageFlags usage) {
  const auto buff_create_info = make_buffer_create_info(size, usage);
  AllocationCreateInfo alloc_info{};
  alloc_info.required_memory_properties = MemoryProperty::HostVisible;
  return create_managed_buffer(allocator, &buff_create_info, &alloc_info);
}

Result<ManagedBuffer>
vk::create_host_visible_host_coherent_buffer(Allocator* allocator,
                                             std::size_t size,
                                             VkBufferUsageFlags usage) {
  const auto buff_create_info = make_buffer_create_info(size, usage);
  AllocationCreateInfo alloc_info{};
  alloc_info.required_memory_properties = MemoryProperty::HostVisible | MemoryProperty::HostCoherent;
  return create_managed_buffer(allocator, &buff_create_info, &alloc_info);
}

Result<ManagedBuffer> vk::create_uniform_buffer(Allocator* allocator, std::size_t size) {
  return create_host_visible_buffer(allocator, size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
}

Result<ManagedBuffer> vk::create_storage_buffer(Allocator* allocator, std::size_t size) {
  return create_host_visible_buffer(allocator, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
}

Result<ManagedBuffer> vk::create_device_local_storage_buffer(Allocator* allocator, std::size_t size) {
  return create_device_local_buffer(allocator, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
}

Result<ManagedBuffer> vk::create_staging_buffer(Allocator* allocator, std::size_t size) {
  return create_host_visible_host_coherent_buffer(
    allocator, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
}

Result<ManagedBuffer> vk::create_host_visible_vertex_buffer(Allocator* allocator, std::size_t size) {
  return create_host_visible_buffer(allocator, size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
}

Result<ManagedBuffer> vk::create_device_local_vertex_buffer(Allocator* allocator,
                                                            size_t size,
                                                            bool transfer_dst) {
  VkBufferUsageFlags usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  if (transfer_dst) {
    usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  }
  return create_device_local_buffer(allocator, size, usage);
}

Result<ManagedBuffer> vk::create_device_local_index_buffer(Allocator* allocator,
                                                           size_t size,
                                                           bool transfer_dst) {
  VkBufferUsageFlags usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  if (transfer_dst) {
    usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  }
  return create_device_local_buffer(allocator, size, usage);
}

Result<ManagedBuffer> vk::create_host_visible_index_buffer(Allocator* allocator, size_t size) {
  return create_host_visible_buffer(allocator, size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

Result<ManagedBuffer> vk::create_dynamic_uniform_buffer(Allocator* allocator,
                                                        size_t min_align,
                                                        std::size_t desired_element_stride,
                                                        std::size_t num_elements,
                                                        std::size_t* actual_element_stride,
                                                        std::size_t* actual_buffer_size) {
  const size_t stride = aligned_element_size_check_zero(desired_element_stride, min_align);
  size_t size = stride * num_elements;
  *actual_element_stride = stride;
  *actual_buffer_size = size;
  return create_uniform_buffer(allocator, size);
}

Result<ManagedBuffer> vk::create_dynamic_storage_buffer(Allocator* allocator,
                                                        size_t min_align,
                                                        std::size_t desired_element_stride,
                                                        std::size_t num_elements,
                                                        std::size_t* actual_element_stride,
                                                        std::size_t* actual_buffer_size) {
  const size_t stride = aligned_element_size_check_zero(desired_element_stride, min_align);
  size_t size = stride * num_elements;
  *actual_element_stride = stride;
  *actual_buffer_size = size;
  return create_storage_buffer(allocator, size);
}

Result<ManagedBuffer>
vk::create_device_local_vertex_buffer_sync(Allocator* allocator,
                                           size_t size,
                                           const void* data,
                                           const Core* core,
                                           CommandProcessor* uploader) {
  assert(data && uploader && core);
  auto buff_res = grove::create_device_local_vertex_buffer(allocator, size, true);
  if (!buff_res) {
    return error_cast<ManagedBuffer>(buff_res);
  }

  auto buff = std::move(buff_res.value);
  VkBuffer dst_buff = buff.contents().buffer.handle;
  if (auto err = copy_staging_buffer_sync(allocator, dst_buff, data, size, core, uploader)) {
    return error_cast<ManagedBuffer>(err);
  }

  return buff;
}

Result<ManagedBuffer>
vk::create_device_local_index_buffer_sync(Allocator* allocator,
                                          size_t size,
                                          const void* data,
                                          const Core* core,
                                          CommandProcessor* uploader) {
  assert(data && uploader && core);
  auto buff_res = grove::create_device_local_index_buffer(allocator, size, true);
  if (!buff_res) {
    return error_cast<ManagedBuffer>(buff_res);
  }

  auto buff = std::move(buff_res.value);
  VkBuffer dst_buff = buff.contents().buffer.handle;
  if (auto err = copy_staging_buffer_sync(allocator, dst_buff, data, size, core, uploader)) {
    return error_cast<ManagedBuffer>(err);
  }

  return buff;
}

GROVE_NAMESPACE_END
