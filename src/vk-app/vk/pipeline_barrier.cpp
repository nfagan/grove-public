#include "pipeline_barrier.hpp"
#include "grove/vk/image.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

using namespace vk;

void cmd::pipeline_barrier(VkCommandBuffer cmd, const PipelineBarrierDescriptor* descriptor) {
  vkCmdPipelineBarrier(
    cmd,
    descriptor->stages.src,
    descriptor->stages.dst,
    descriptor->dependency_flags,
    descriptor->num_memory_barriers,
    descriptor->memory_barriers,
    descriptor->num_buffer_memory_barriers,
    descriptor->buffer_memory_barriers,
    descriptor->num_image_memory_barriers,
    descriptor->image_memory_barriers);
}

ImageMemoryBarrierDescriptor
vk::make_image_memory_barrier_descriptor(VkPipelineStageFlags src,
                                         VkPipelineStageFlags dst,
                                         VkImageMemoryBarrier barrier,
                                         VkDependencyFlags depend_flags) {
  ImageMemoryBarrierDescriptor res{};
  res.stages.src = src;
  res.stages.dst = dst;
  res.barrier = barrier;
  res.dependency_flags = depend_flags;
  return res;
}

VkImageSubresourceRange
vk::make_color_aspect_image_subresource_range(uint32_t layer,
                                              uint32_t num_layers,
                                              uint32_t mip,
                                              uint32_t num_mips) {
  VkImageSubresourceRange range{};
  range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  range.baseMipLevel = mip;
  range.levelCount = num_mips;
  range.baseArrayLayer = layer;
  range.layerCount = num_layers;
  return range;
}

VkImageMemoryBarrier
vk::make_undefined_to_transfer_dst_image_memory_barrier(VkImage image,
                                                        VkImageSubresourceRange range,
                                                        VkImageLayout old_layout,
                                                        VkImageLayout new_layout) {
  VkImageMemoryBarrier barrier = make_empty_image_memory_barrier();
  barrier.srcAccessMask = 0;
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.oldLayout = old_layout;
  barrier.newLayout = new_layout;
  barrier.image = image;
  barrier.subresourceRange = range;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  return barrier;
}

VkImageMemoryBarrier
vk::make_undefined_to_shader_read_only_image_memory_barrier(VkImage image,
                                                            VkImageSubresourceRange range,
                                                            VkImageLayout old_layout,
                                                            VkImageLayout new_layout) {
  VkImageMemoryBarrier barrier = make_empty_image_memory_barrier();
  barrier.srcAccessMask = 0;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.oldLayout = old_layout;
  barrier.newLayout = new_layout;
  barrier.image = image;
  barrier.subresourceRange = range;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  return barrier;
}

VkImageMemoryBarrier vk::make_transfer_dst_to_shader_read_only_image_memory_barrier(VkImage image,
                                                                                    VkImageSubresourceRange range,
                                                                                    VkImageLayout old_layout,
                                                                                    VkImageLayout new_layout) {
  VkImageMemoryBarrier barrier = make_empty_image_memory_barrier();
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.oldLayout = old_layout;
  barrier.newLayout = new_layout;
  barrier.image = image;
  barrier.subresourceRange = range;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  return barrier;
}

GROVE_NAMESPACE_END
