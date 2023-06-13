#pragma once

#include "common.hpp"

namespace grove::vk {

struct RenderPass {
  VkRenderPass handle{VK_NULL_HANDLE};
};

Result<RenderPass> create_render_pass(VkDevice device, const VkRenderPassCreateInfo* create_info);
Result<RenderPass> create_render_pass2(VkInstance instance,
                                       VkDevice device,
                                       const VkRenderPassCreateInfo2* create_info);
void destroy_render_pass(RenderPass* pass, VkDevice device);

inline VkRenderPassCreateInfo make_empty_render_pass_create_info() {
  VkRenderPassCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  return create_info;
}

inline VkRenderPassBeginInfo make_empty_render_pass_begin_info() {
  VkRenderPassBeginInfo result{};
  result.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  return result;
}

}