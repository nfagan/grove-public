#pragma once

#include "common.hpp"
#include "command_buffer.hpp"
#include <vector>

namespace grove::vk {

struct CommandPool {
  const CommandBuffer* ith_command_buffer(uint32_t i) const {
    return i < uint32_t(command_buffers.size()) ? &command_buffers[i] : nullptr;
  }
  uint32_t num_command_buffers() const {
    return uint32_t(command_buffers.size());
  }

  VkCommandPool handle{VK_NULL_HANDLE};
  uint32_t queue_family{};
  std::vector<CommandBuffer> command_buffers;
};

VkCommandPoolCreateInfo make_command_pool_create_info(uint32_t queue_family,
                                                      VkCommandPoolCreateFlags flags);

Result<CommandPool> create_command_pool(VkDevice device,
                                        const VkCommandPoolCreateInfo* create_info);
Result<CommandPool> create_command_pool(VkDevice device,
                                        uint32_t queue_family,
                                        uint32_t num_buffers_alloc,
                                        VkCommandPoolCreateFlags pool_flags = 0,
                                        VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

void destroy_command_pool(CommandPool* pool, VkDevice device);
void destroy_command_pools(std::vector<CommandPool>* pools, VkDevice device);

inline void reset_command_pool(VkDevice device, VkCommandPool pool, VkCommandPoolResetFlags flags = 0) {
  GROVE_VK_CHECK(vkResetCommandPool(device, pool, flags))
}

VkCommandBufferAllocateInfo
make_command_buffer_allocate_info(VkCommandPool pool,
                                  uint32_t num_allocate,
                                  VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

[[nodiscard]] Error allocate_command_buffers(VkDevice device,
                                             const VkCommandBufferAllocateInfo* alloc_info,
                                             VkCommandBuffer* buffers);

Result<std::vector<CommandBuffer>>
allocate_command_buffers(VkDevice device, const VkCommandBufferAllocateInfo* alloc_info);

}