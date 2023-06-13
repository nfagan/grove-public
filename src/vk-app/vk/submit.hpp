#pragma once

#include "grove/vk/common.hpp"
#include <functional>

namespace grove::vk {

inline VkSubmitInfo make_empty_submit_info() {
  VkSubmitInfo result{};
  result.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  return result;
}

[[nodiscard]] Error immediate_submit(VkDevice device,
                                     VkQueue queue,
                                     VkCommandPool pool,
                                     VkCommandBuffer cmd,
                                     VkFence fence,
                                     std::function<void(VkCommandBuffer)>&& f);

[[nodiscard]] Error queue_submit(VkQueue queue,
                                 uint32_t submit_count,
                                 const VkSubmitInfo* info,
                                 VkFence fence);

[[nodiscard]] Error submit_sync(VkDevice device,
                                VkCommandBuffer buff,
                                VkQueue queue,
                                VkFence fence);

[[nodiscard]] Error queue_submit(VkCommandBuffer buff, VkQueue queue, VkFence fence);

}