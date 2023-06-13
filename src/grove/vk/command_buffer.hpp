#pragma once

#include "common.hpp"

namespace grove::vk {

struct CommandBuffer {
  VkCommandBuffer handle{VK_NULL_HANDLE};
};

inline VkCommandBufferBeginInfo make_empty_command_buffer_begin_info() {
  VkCommandBufferBeginInfo result{};
  result.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  return result;
}

inline VkCommandBufferBeginInfo make_command_buffer_begin_info(VkCommandBufferUsageFlags usage_flags) {
  VkCommandBufferBeginInfo result{};
  result.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  result.flags = usage_flags;
  return result;
}

[[nodiscard]] Error begin_command_buffer(VkCommandBuffer handle,
                                         const VkCommandBufferBeginInfo* info);
[[nodiscard]] Error end_command_buffer(VkCommandBuffer handle);

}