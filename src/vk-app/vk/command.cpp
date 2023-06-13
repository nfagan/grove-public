#include "command.hpp"
#include "grove/common/common.hpp"
#include "grove/vk/core.hpp"

GROVE_NAMESPACE_BEGIN

void vk::cmd::copy_buffer(VkCommandBuffer cmd, VkBuffer src, VkBuffer dst,
                          size_t size, size_t src_off, size_t dst_off) {

  VkBufferCopy copy{};
  copy.size = size;
  copy.srcOffset = src_off;
  copy.dstOffset = dst_off;
  vkCmdCopyBuffer(cmd, src, dst, 1, &copy);
}

void vk::cmd::push_descriptor_set(VkInstance instance, VkCommandBuffer cmd,
                                  VkPipelineBindPoint pipeline_bind_point,
                                  VkPipelineLayout layout, uint32_t set,
                                  const VkWriteDescriptorSet* writes, uint32_t num_writes) {
  static PFN_vkCmdPushDescriptorSetKHR func{};
  if (!func) {
    func = (PFN_vkCmdPushDescriptorSetKHR) vkGetInstanceProcAddr(instance, "vkCmdPushDescriptorSetKHR");
  }
  assert(func);
  func(cmd, pipeline_bind_point, layout, set, num_writes, writes);
}

void vk::cmd::push_graphics_descriptor_set(const Core& core, VkCommandBuffer cmd,
                                           VkPipelineLayout layout, uint32_t set,
                                           const VkWriteDescriptorSet* writes,
                                           uint32_t num_writes) {
  auto bp = VK_PIPELINE_BIND_POINT_GRAPHICS;
  push_descriptor_set(core.instance.handle, cmd, bp, layout, set, writes, num_writes);
}

GROVE_NAMESPACE_END
