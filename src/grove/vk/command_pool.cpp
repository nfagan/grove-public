#include "command_pool.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

VkCommandPoolCreateInfo vk::make_command_pool_create_info(uint32_t queue_family,
                                                          VkCommandPoolCreateFlags flags) {
  VkCommandPoolCreateInfo result{};
  result.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  result.queueFamilyIndex = queue_family;
  result.flags = flags;
  return result;
}

vk::Result<vk::CommandPool> vk::create_command_pool(VkDevice device,
                                                    const VkCommandPoolCreateInfo* create_info) {
  VkCommandPool handle{};
  auto res = vkCreateCommandPool(device, create_info, GROVE_VK_ALLOC, &handle);
  if (res != VK_SUCCESS) {
    return {res, "Failed to create command pool."};
  } else {
    CommandPool pool{};
    pool.handle = handle;
    pool.queue_family = create_info->queueFamilyIndex;
    return pool;
  }
}

vk::Result<vk::CommandPool> vk::create_command_pool(VkDevice device,
                                                    uint32_t queue_family,
                                                    uint32_t num_buffers_alloc,
                                                    VkCommandPoolCreateFlags pool_flags,
                                                    VkCommandBufferLevel level) {
  auto create_info = vk::make_command_pool_create_info(queue_family, pool_flags);
  auto res = vk::create_command_pool(device, &create_info);
  if (!res) {
    return res;
  }
  auto pool = std::move(res.value);
  auto alloc_info = vk::make_command_buffer_allocate_info(pool.handle, num_buffers_alloc, level);
  if (auto alloc_res = vk::allocate_command_buffers(device, &alloc_info)) {
    //  Ok.
    pool.command_buffers = std::move(alloc_res.value);
    return pool;
  } else {
    //  Failed to allocate.
    destroy_command_pool(&pool, device);
    return error_cast<CommandPool>(alloc_res);
  }
}

void vk::destroy_command_pool(CommandPool* pool, VkDevice device) {
  if (device != VK_NULL_HANDLE) {
    vkDestroyCommandPool(device, pool->handle, GROVE_VK_ALLOC);
    pool->handle = VK_NULL_HANDLE;
    pool->queue_family = 0;
    pool->command_buffers.clear();
  } else {
    GROVE_ASSERT(pool->handle == VK_NULL_HANDLE && pool->command_buffers.empty());
  }
}

void vk::destroy_command_pools(std::vector<CommandPool>* pools, VkDevice device) {
  for (auto& pool : *pools) {
    destroy_command_pool(&pool, device);
  }
  pools->clear();
}

VkCommandBufferAllocateInfo vk::make_command_buffer_allocate_info(VkCommandPool pool,
                                                                  uint32_t num_allocate,
                                                                  VkCommandBufferLevel level) {
  VkCommandBufferAllocateInfo result{};
  result.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  result.commandBufferCount = num_allocate;
  result.commandPool = pool;
  result.level = level;
  return result;
}

[[nodiscard]] vk::Error vk::allocate_command_buffers(VkDevice device,
                                                     const VkCommandBufferAllocateInfo* alloc_info,
                                                     VkCommandBuffer* buffers) {
  auto res = vkAllocateCommandBuffers(device, alloc_info, buffers);
  if (res != VK_SUCCESS) {
    return {res, "Failed to allocate command buffers."};
  } else {
    return {};
  }
}

vk::Result<std::vector<vk::CommandBuffer>>
vk::allocate_command_buffers(VkDevice device, const VkCommandBufferAllocateInfo* alloc_info) {
  std::vector<VkCommandBuffer> handles(alloc_info->commandBufferCount);
  if (auto err = allocate_command_buffers(device, alloc_info, handles.data())) {
    return error_cast<std::vector<CommandBuffer>>(err);
  } else {
    std::vector<CommandBuffer> result;
    for (auto& handle : handles) {
      CommandBuffer buff;
      buff.handle = handle;
      result.push_back(buff);
    }
    return result;
  }
}

GROVE_NAMESPACE_END
