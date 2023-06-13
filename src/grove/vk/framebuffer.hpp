#pragma once

#include "common.hpp"

namespace grove::vk {

struct Framebuffer {
  VkFramebuffer handle{VK_NULL_HANDLE};
};

Result<Framebuffer> create_framebuffer(VkDevice device, const VkFramebufferCreateInfo* create_info);
void destroy_framebuffer(vk::Framebuffer* framebuffer, VkDevice device);

inline VkFramebufferCreateInfo make_empty_framebuffer_create_info() {
  VkFramebufferCreateInfo result{};
  result.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  return result;
}

}