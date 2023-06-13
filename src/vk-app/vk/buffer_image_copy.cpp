#include "buffer_image_copy.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

using namespace vk;

namespace {

PipelineBarrierDescriptor to_pipeline_barrier_descriptor(const ImageMemoryBarrierDescriptor* descr) {
  PipelineBarrierDescriptor result{};
  result.stages = descr->stages;
  result.image_memory_barriers = &descr->barrier;
  result.num_image_memory_barriers = 1;
  result.dependency_flags = descr->dependency_flags;
  return result;
}

CopyBufferToImageDescriptor
to_copy_buffer_to_image_descriptor(const CopyBufferToImageDescriptorOneRegion* descr) {
  CopyBufferToImageDescriptor copy_descr{};
  copy_descr.src = descr->src;
  copy_descr.dst = descr->dst;
  copy_descr.dst_layout = descr->dst_layout;
  copy_descr.regions = &descr->region;
  copy_descr.num_regions = 1;
  return copy_descr;
}

VkImageSubresourceLayers make_image_subresource_layers(VkImageAspectFlags aspect, uint32_t mip,
                                                       uint32_t layer, uint32_t num_layers) {
  VkImageSubresourceLayers result{};
  result.aspectMask = aspect;
  result.mipLevel = mip;
  result.baseArrayLayer = layer;
  result.layerCount = num_layers;
  return result;
}

VkImageSubresourceLayers to_image_subresource_layers(const VkImageSubresourceRange& range) {
  return make_image_subresource_layers(
    range.aspectMask,
    range.baseMipLevel,
    range.baseArrayLayer,
    range.layerCount);
}

CopyBufferToImageDescriptorOneRegion
make_copy_buffer_to_image_descriptor_one_region(VkBuffer src, VkImage dst, VkImageLayout dst_layout,
                                                VkBufferImageCopy region) {
  CopyBufferToImageDescriptorOneRegion descr{};
  descr.src = src;
  descr.dst = dst;
  descr.dst_layout = dst_layout;
  descr.region = region;
  return descr;
}

} //  anon

void vk::cmd::copy_buffer_to_image(VkCommandBuffer cmd, const CopyBufferToImageDescriptor* descr) {
  vkCmdCopyBufferToImage(
    cmd, descr->src, descr->dst, descr->dst_layout, descr->num_regions, descr->regions);
}

void vk::cmd::copy_buffer_to_image(VkCommandBuffer cmd,
                                   const PipelineBarrierDescriptor* copy_barrier,
                                   const CopyBufferToImageDescriptor* copy_descriptor,
                                   const PipelineBarrierDescriptor* read_barrier) {
  pipeline_barrier(cmd, copy_barrier);
  copy_buffer_to_image(cmd, copy_descriptor);
  pipeline_barrier(cmd, read_barrier);
}

void vk::cmd::buffer_image_copy(VkCommandBuffer cmd, const BufferImageCopy* descriptor) {
  const PipelineBarrierDescriptor copy_barrier_descr =
    to_pipeline_barrier_descriptor(&descriptor->copy_barrier);
  const CopyBufferToImageDescriptor copy_descr =
    to_copy_buffer_to_image_descriptor(&descriptor->copy_buffer_to_image);
  const PipelineBarrierDescriptor read_barrier_descr =
    to_pipeline_barrier_descriptor(&descriptor->read_barrier);
  copy_buffer_to_image(cmd, &copy_barrier_descr, &copy_descr, &read_barrier_descr);
}

BufferImageCopy vk::make_buffer_image_copy_shader_read_only_dst(VkImage image,
                                                                VkBuffer buffer,
                                                                const VkExtent3D& image_extent,
                                                                const VkImageSubresourceRange& subresource_range,
                                                                VkPipelineStageFlags read_dst_stage) {
  BufferImageCopy copy{};
  copy.copy_buffer_to_image = make_copy_buffer_to_image_descriptor_one_region(
    buffer,
    image,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    make_buffer_image_copy(to_image_subresource_layers(subresource_range), image_extent));

  copy.copy_barrier = make_image_memory_barrier_descriptor(
    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    VK_PIPELINE_STAGE_TRANSFER_BIT,
    make_undefined_to_transfer_dst_image_memory_barrier(image, subresource_range),
    0);

  copy.read_barrier = make_image_memory_barrier_descriptor(
    VK_PIPELINE_STAGE_TRANSFER_BIT,
    read_dst_stage,
    make_transfer_dst_to_shader_read_only_image_memory_barrier(image, subresource_range),
    0);
  return copy;
}

BufferImageCopy
vk::make_buffer_image_copy_shader_read_only_dst(const Image& image,
                                                VkBuffer buffer,
                                                const VkImageSubresourceRange& subresource_range,
                                                VkPipelineStageFlags read_dst_stage) {
  return make_buffer_image_copy_shader_read_only_dst(
    image.handle, buffer, image.extent, subresource_range, read_dst_stage);
}

VkBufferImageCopy vk::make_buffer_image_copy(const VkImageSubresourceLayers& subresource,
                                             const VkExtent3D& image_extent,
                                             const VkOffset3D& image_offset,
                                             size_t buff_off,
                                             uint32_t buffer_row_len,
                                             uint32_t buffer_im_height) {
  VkBufferImageCopy result{};
  result.bufferOffset = buff_off;
  result.bufferRowLength = buffer_row_len;
  result.bufferImageHeight = buffer_im_height;
  result.imageSubresource = subresource;
  result.imageOffset = image_offset;
  result.imageExtent = image_extent;
  return result;
}

GROVE_NAMESPACE_END
