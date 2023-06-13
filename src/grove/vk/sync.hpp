#pragma once

#include "common.hpp"
#include <vector>

namespace grove::vk {

struct Semaphore {
  VkSemaphore handle{VK_NULL_HANDLE};
};

struct Fence {
  VkFence handle{VK_NULL_HANDLE};
};

Result<Fence> create_fence(VkDevice device, VkFenceCreateFlags flags);
Result<std::vector<Fence>> create_fences(VkDevice device, uint32_t count, VkFenceCreateFlags flags);

void destroy_fence(vk::Fence* fence, VkDevice device);
void destroy_fences(std::vector<vk::Fence>* fences, VkDevice device);

Result<Semaphore> create_semaphore(VkDevice device, VkSemaphoreCreateFlags flags = 0);
Result<std::vector<Semaphore>> create_semaphores(VkDevice device,
                                                 uint32_t count,
                                                 VkSemaphoreCreateFlags flags = 0);

void destroy_semaphore(vk::Semaphore* sema, VkDevice device);
void destroy_semaphores(std::vector<vk::Semaphore>* semaphores, VkDevice device);

[[nodiscard]] Error wait_fences(VkDevice device,
                                uint32_t count,
                                const VkFence* fences,
                                bool wait_all,
                                uint64_t timeout);
[[nodiscard]] Error wait_fence(VkDevice device, VkFence fence, uint64_t timeout);
void reset_fences(VkDevice device, uint32_t count, const VkFence* fences);

}