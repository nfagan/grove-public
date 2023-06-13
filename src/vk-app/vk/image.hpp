#pragma once

#include "grove/vk/vk.hpp"

namespace grove::vk {

constexpr VkImageUsageFlags sampled_or_transfer_dst() {
  return VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
}

VkImageViewCreateInfo make_image_view_create_info(VkImage image,
                                                  VkImageViewType type,
                                                  VkFormat format,
                                                  VkComponentMapping mapping,
                                                  VkImageSubresourceRange subresource_range,
                                                  VkImageViewCreateFlags flags = 0);

VkImageCreateInfo make_image_create_info(VkImageType type,
                                         VkFormat format,
                                         VkExtent3D extent,
                                         VkImageUsageFlags usage,
                                         VkImageTiling tiling,
                                         uint32_t array_layers,
                                         uint32_t mip_levels,
                                         VkSampleCountFlagBits samples,
                                         VkSharingMode sharing_mode);

VkImageCreateInfo make_2d_image_create_info(VkFormat format,
                                            VkExtent2D extent,
                                            VkImageUsageFlags usage,
                                            VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL,
                                            uint32_t array_layers = 1,
                                            uint32_t mip_levels = 1,
                                            VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
                                            VkSharingMode sharing_mode = VK_SHARING_MODE_EXCLUSIVE);

Result<ManagedImage> create_device_local_image(Allocator* allocator, const VkImageCreateInfo* info);

}