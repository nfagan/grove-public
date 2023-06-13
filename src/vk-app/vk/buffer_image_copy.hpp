#pragma once

#include "pipeline_barrier.hpp"
#include "grove/vk/image.hpp"
#include "grove/vk/buffer.hpp"

namespace grove::vk {

struct CopyBufferToImageDescriptor {
  VkBuffer src;
  VkImage dst;
  VkImageLayout dst_layout;
  const VkBufferImageCopy* regions;
  uint32_t num_regions;
};

struct CopyBufferToImageDescriptorOneRegion {
  VkBuffer src;
  VkImage dst;
  VkImageLayout dst_layout;
  VkBufferImageCopy region;
};

struct BufferImageCopy {
  ImageMemoryBarrierDescriptor copy_barrier;
  CopyBufferToImageDescriptorOneRegion copy_buffer_to_image;
  ImageMemoryBarrierDescriptor read_barrier;
};

namespace cmd {

void copy_buffer_to_image(VkCommandBuffer cmd, const CopyBufferToImageDescriptor* descriptor);
void copy_buffer_to_image(VkCommandBuffer cmd,
                          const PipelineBarrierDescriptor* copy_barrier,
                          const CopyBufferToImageDescriptor* copy_descriptor,
                          const PipelineBarrierDescriptor* read_barrier);
void buffer_image_copy(VkCommandBuffer cmd, const BufferImageCopy* descriptor);

} //  cmd

BufferImageCopy make_buffer_image_copy_shader_read_only_dst(VkImage image,
                                                            VkBuffer buffer,
                                                            const VkExtent3D& image_extent,
                                                            const VkImageSubresourceRange& subresource_range,
                                                            VkPipelineStageFlags read_dst_stage);

BufferImageCopy make_buffer_image_copy_shader_read_only_dst(const Image& image,
                                                            VkBuffer buffer,
                                                            const VkImageSubresourceRange& subresource_range,
                                                            VkPipelineStageFlags read_dst_stage);

VkBufferImageCopy make_buffer_image_copy(const VkImageSubresourceLayers& subresource,
                                         const VkExtent3D& image_extent,
                                         const VkOffset3D& image_offset = {},
                                         size_t buff_off = 0,
                                         uint32_t buffer_row_len = 0,
                                         uint32_t buffer_im_height = 0);

}