#include "submit.hpp"
#include "grove/common/common.hpp"
#include "grove/vk/command_buffer.hpp"
#include "grove/vk/command_pool.hpp"
#include "grove/vk/sync.hpp"

GROVE_NAMESPACE_BEGIN

using namespace vk;

[[nodiscard]] Error vk::submit_sync(VkDevice device,
                                    VkCommandBuffer buff,
                                    VkQueue queue,
                                    VkFence fence) {
  auto submit_info = vk::make_empty_submit_info();
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &buff;
  GROVE_VK_TRY_ERR(vk::queue_submit(queue, 1, &submit_info, fence))
  GROVE_VK_TRY_ERR(vk::wait_fence(device, fence, UINT64_MAX))
  vk::reset_fences(device, 1, &fence);
  return {};
}

[[nodiscard]] Error vk::queue_submit(VkCommandBuffer buff, VkQueue queue, VkFence fence) {
  auto submit_info = vk::make_empty_submit_info();
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &buff;
  GROVE_VK_TRY_ERR(vk::queue_submit(queue, 1, &submit_info, fence))
  return {};
}

[[nodiscard]] Error vk::queue_submit(VkQueue queue,
                                     uint32_t submit_count,
                                     const VkSubmitInfo* info,
                                     VkFence fence) {
  auto res = vkQueueSubmit(queue, submit_count, info, fence);
  if (res != VK_SUCCESS) {
    return {res, "Failed to submit to queue."};
  } else {
    return {};
  }
}

[[nodiscard]] Error vk::immediate_submit(VkDevice device,
                                         VkQueue queue,
                                         VkCommandPool pool,
                                         VkCommandBuffer cmd,
                                         VkFence fence,
                                         std::function<void(VkCommandBuffer)>&& f) {
  const auto info = vk::make_command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  GROVE_VK_TRY_ERR(vk::begin_command_buffer(cmd, &info))
  f(cmd);
  GROVE_VK_TRY_ERR(vk::end_command_buffer(cmd))
  GROVE_VK_TRY_ERR(vk::submit_sync(device, cmd, queue, fence))
  vk::reset_command_pool(device, pool);
  return {};
}

GROVE_NAMESPACE_END
