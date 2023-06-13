#include "command_buffer.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

[[nodiscard]] vk::Error vk::begin_command_buffer(VkCommandBuffer handle,
                                                 const VkCommandBufferBeginInfo* info) {
  if (auto res = vkBeginCommandBuffer(handle, info); res != VK_SUCCESS) {
    return {res, "Failed to begin command buffer."};
  } else {
    return {};
  }
}

[[nodiscard]] vk::Error vk::end_command_buffer(VkCommandBuffer handle) {
  if (auto res = vkEndCommandBuffer(handle); res != VK_SUCCESS) {
    return {res, "Failed to end command buffer."};
  } else {
    return {};
  }
}

GROVE_NAMESPACE_END
