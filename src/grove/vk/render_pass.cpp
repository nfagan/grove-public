#include "render_pass.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

vk::Result<vk::RenderPass> vk::create_render_pass(VkDevice device,
                                                  const VkRenderPassCreateInfo* create_info) {
  VkRenderPass render_pass;
  auto create_res = vkCreateRenderPass(device, create_info, GROVE_VK_ALLOC, &render_pass);
  if (create_res != VK_SUCCESS) {
    return {create_res, "Failed to create render pass."};
  } else {
    vk::RenderPass result{};
    result.handle = render_pass;
    return result;
  }
}

vk::Result<vk::RenderPass> vk::create_render_pass2(VkInstance instance,
                                                   VkDevice device,
                                                   const VkRenderPassCreateInfo2* create_info) {
  auto func = (PFN_vkCreateRenderPass2KHR) vkGetInstanceProcAddr(instance, "vkCreateRenderPass2KHR");
  if (!func) {
    return {VK_ERROR_EXTENSION_NOT_PRESENT, "Failed to load vkCreateRenderPass2KHR"};
  }

  VkRenderPass render_pass;
  auto create_res = func(device, create_info, GROVE_VK_ALLOC, &render_pass);
  if (create_res != VK_SUCCESS) {
    return {create_res, "Failed to create render pass."};
  } else {
    vk::RenderPass result{};
    result.handle = render_pass;
    return result;
  }
}

void vk::destroy_render_pass(vk::RenderPass* render_pass, VkDevice device) {
  if (device != VK_NULL_HANDLE) {
    vkDestroyRenderPass(device, render_pass->handle, GROVE_VK_ALLOC);
    render_pass->handle = VK_NULL_HANDLE;
  } else {
    assert(render_pass->handle == VK_NULL_HANDLE);
  }
}

GROVE_NAMESPACE_END
