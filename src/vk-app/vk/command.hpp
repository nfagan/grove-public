#pragma once

#include "grove/vk/command_buffer.hpp"

namespace grove::vk {
struct Core;
}

namespace grove::vk::cmd {

inline void bind_graphics_pipeline(VkCommandBuffer cmd, VkPipeline pipeline) {
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}

inline void bind_compute_pipeline(VkCommandBuffer cmd, VkPipeline pipeline) {
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
}

inline void bind_graphics_descriptor_sets(VkCommandBuffer cmd, VkPipelineLayout layout,
                                          uint32_t first_set, uint32_t num_sets,
                                          const VkDescriptorSet* sets,
                                          uint32_t num_dyn_offsets = 0,
                                          const uint32_t* dyn_offsets = nullptr) {
  vkCmdBindDescriptorSets(
    cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, first_set,
    num_sets, sets, num_dyn_offsets, dyn_offsets);
}

inline void bind_compute_descriptor_sets(VkCommandBuffer cmd, VkPipelineLayout layout,
                                         uint32_t first_set, uint32_t num_sets,
                                         const VkDescriptorSet* sets,
                                         uint32_t num_dyn_offsets = 0,
                                         const uint32_t* dyn_offsets = nullptr) {
  vkCmdBindDescriptorSets(
    cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, first_set,
    num_sets, sets, num_dyn_offsets, dyn_offsets);
}

inline void set_viewport_and_scissor(VkCommandBuffer cmd,
                                     const VkViewport* viewport,
                                     const VkRect2D* scissor) {
  vkCmdSetViewport(cmd, 0, 1, viewport);
  vkCmdSetScissor(cmd, 0, 1, scissor);
}

inline void pipeline_barrier_image_memory_barrier(VkCommandBuffer cmd,
                                                  VkShaderStageFlags src_stage,
                                                  VkShaderStageFlags dst_stage,
                                                  const VkImageMemoryBarrier* barriers,
                                                  uint32_t num_barriers) {
  vkCmdPipelineBarrier(
    cmd, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, num_barriers, barriers);
}

void copy_buffer(VkCommandBuffer cmd, VkBuffer src, VkBuffer dst, size_t size,
                 size_t src_off = 0, size_t dst_off = 0);

template <typename T>
void push_constants(VkCommandBuffer cmd,
                    VkPipelineLayout layout,
                    VkShaderStageFlags stages,
                    const T* data,
                    uint32_t offset = 0) {
  vkCmdPushConstants(cmd, layout, stages, offset, sizeof(T), data);
};

void push_descriptor_set(VkInstance instance, VkCommandBuffer cmd,
                         VkPipelineBindPoint pipeline_bind_point,
                         VkPipelineLayout layout, uint32_t set, const VkWriteDescriptorSet* writes,
                         uint32_t num_writes);

void push_graphics_descriptor_set(const Core& core, VkCommandBuffer cmd,
                                  VkPipelineLayout layout, uint32_t set,
                                  const VkWriteDescriptorSet* writes, uint32_t num_writes);

template <typename T>
void push_graphics_descriptor_set(const Core& core, VkCommandBuffer cmd, VkPipelineLayout layout,
                                  uint32_t set, const T& writes) {
  push_graphics_descriptor_set(core, cmd, layout, set, writes.writes.data(), writes.num_writes);
}

}