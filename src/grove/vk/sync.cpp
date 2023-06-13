#include "sync.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

vk::Result<vk::Fence> vk::create_fence(VkDevice device, VkFenceCreateFlags flags) {
  VkFenceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  create_info.flags = flags;
  VkFence handle;
  auto res = vkCreateFence(device, &create_info, GROVE_VK_ALLOC, &handle);
  if (res != VK_SUCCESS) {
    return {res, "Failed to create fence."};
  } else {
    Fence fence{};
    fence.handle = handle;
    return fence;
  }
}

vk::Result<std::vector<vk::Fence>>
vk::create_fences(VkDevice device, uint32_t count, VkFenceCreateFlags flags) {
  std::vector<Fence> result;
  for (uint32_t i = 0; i < count; i++) {
    if (auto res = create_fence(device, flags)) {
      result.push_back(res.value);
    } else {
      destroy_fences(&result, device);
      return error_cast<std::vector<Fence>>(res);
    }
  }
  return result;
}

vk::Result<vk::Semaphore> vk::create_semaphore(VkDevice device, VkSemaphoreCreateFlags flags) {
  VkSemaphoreCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  create_info.flags = flags;
  VkSemaphore handle;
  auto res = vkCreateSemaphore(device, &create_info, GROVE_VK_ALLOC, &handle);
  if (res != VK_SUCCESS) {
    return {res, "Failed to create semaphore."};
  } else {
    Semaphore result{};
    result.handle = handle;
    return result;
  }
}

vk::Result<std::vector<vk::Semaphore>> vk::create_semaphores(VkDevice device,
                                                             uint32_t count,
                                                             VkSemaphoreCreateFlags flags) {
  std::vector<Semaphore> result;
  for (uint32_t i = 0; i < count; i++) {
    if (auto res = create_semaphore(device, flags)) {
      result.push_back(res.value);
    } else {
      destroy_semaphores(&result, device);
      return error_cast<std::vector<Semaphore>>(res);
    }
  }
  return result;
}

void vk::destroy_fence(vk::Fence* fence, VkDevice device) {
  if (device != VK_NULL_HANDLE) {
    vkDestroyFence(device, fence->handle, GROVE_VK_ALLOC);
    fence->handle = VK_NULL_HANDLE;
  } else {
    GROVE_ASSERT(fence->handle == VK_NULL_HANDLE);
  }
}

void vk::destroy_fences(std::vector<vk::Fence>* fences, VkDevice device) {
  for (auto& fence : *fences) {
    destroy_fence(&fence, device);
  }
  *fences = {};
}

void vk::destroy_semaphore(vk::Semaphore* sema, VkDevice device) {
  if (device != VK_NULL_HANDLE) {
    vkDestroySemaphore(device, sema->handle, GROVE_VK_ALLOC);
    sema->handle = VK_NULL_HANDLE;
  } else {
    GROVE_ASSERT(sema->handle == VK_NULL_HANDLE);
  }
}

void vk::destroy_semaphores(std::vector<vk::Semaphore>* semaphores, VkDevice device) {
  for (auto& sema : *semaphores) {
    destroy_semaphore(&sema, device);
  }
  *semaphores = {};
}

[[nodiscard]] vk::Error vk::wait_fences(VkDevice device,
                                        uint32_t count,
                                        const VkFence* fences,
                                        bool wait_all,
                                        uint64_t timeout) {
  auto res = vkWaitForFences(device, count, fences, (VkBool32) wait_all, timeout);
  if (res != VK_SUCCESS) {
    return {res, "Failed to wait for fences."};
  } else {
    return {};
  }
}

[[nodiscard]] vk::Error vk::wait_fence(VkDevice device, VkFence fence, uint64_t timeout) {
  auto res = vkWaitForFences(device, 1, &fence, VK_TRUE, timeout);
  if (res != VK_SUCCESS) {
    return {res, "Failed to wait for fence."};
  } else {
    return {};
  }
}

void vk::reset_fences(VkDevice device, uint32_t count, const VkFence* fences) {
  GROVE_VK_CHECK(vkResetFences(device, count, fences))
}

GROVE_NAMESPACE_END
