#pragma once

#include "common.hpp"

namespace grove::vk {

struct PipelineBarrierStages {
  VkPipelineStageFlags src;
  VkPipelineStageFlags dst;
};

struct ImageMemoryBarrierDescriptor {
  PipelineBarrierStages stages;
  VkImageMemoryBarrier barrier;
  VkDependencyFlags dependency_flags;
};

struct PipelineBarrierDescriptor {
  PipelineBarrierStages stages;
  VkDependencyFlags dependency_flags;
  const VkMemoryBarrier* memory_barriers;
  uint32_t num_memory_barriers;
  const VkBufferMemoryBarrier* buffer_memory_barriers;
  uint32_t num_buffer_memory_barriers;
  const VkImageMemoryBarrier* image_memory_barriers;
  uint32_t num_image_memory_barriers;
};

namespace cmd {

void pipeline_barrier(VkCommandBuffer cmd, const PipelineBarrierDescriptor* descriptor);

} //  cmd

ImageMemoryBarrierDescriptor make_image_memory_barrier_descriptor(VkPipelineStageFlags src,
                                                                  VkPipelineStageFlags dst,
                                                                  VkImageMemoryBarrier barrier,
                                                                  VkDependencyFlags depend_flags);

VkImageSubresourceRange make_color_aspect_image_subresource_range(uint32_t layer = 0,
                                                                  uint32_t num_layers = 1,
                                                                  uint32_t mip = 0,
                                                                  uint32_t num_mips = 1);

VkImageMemoryBarrier
make_undefined_to_transfer_dst_image_memory_barrier(VkImage image,
                                                    VkImageSubresourceRange range,
                                                    VkImageLayout old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                                                    VkImageLayout new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

VkImageMemoryBarrier
make_undefined_to_shader_read_only_image_memory_barrier(VkImage image,
                                                        VkImageSubresourceRange range,
                                                        VkImageLayout old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                                                        VkImageLayout new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

VkImageMemoryBarrier
make_transfer_dst_to_shader_read_only_image_memory_barrier(VkImage image,
                                                           VkImageSubresourceRange range,
                                                           VkImageLayout old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                           VkImageLayout new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

}