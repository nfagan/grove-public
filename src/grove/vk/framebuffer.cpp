#include "framebuffer.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

vk::Result<vk::Framebuffer> vk::create_framebuffer(VkDevice device,
                                                   const VkFramebufferCreateInfo* create_info) {
  VkFramebuffer handle;
  auto res = vkCreateFramebuffer(device, create_info, GROVE_VK_ALLOC, &handle);
  if (res != VK_SUCCESS) {
    return {res, "Failed to create framebuffer."};
  } else {
    Framebuffer result{};
    result.handle = handle;
    return result;
  }
}

void vk::destroy_framebuffer(vk::Framebuffer* framebuffer, VkDevice device) {
  if (device != VK_NULL_HANDLE) {
    vkDestroyFramebuffer(device, framebuffer->handle, GROVE_VK_ALLOC);
    framebuffer->handle = VK_NULL_HANDLE;
  } else {
    assert(framebuffer->handle == VK_NULL_HANDLE);
  }
}

GROVE_NAMESPACE_END
