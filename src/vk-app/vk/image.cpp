#include "image.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

using namespace vk;

VkImageViewCreateInfo vk::make_image_view_create_info(VkImage image,
                                                      VkImageViewType type,
                                                      VkFormat format,
                                                      VkComponentMapping components,
                                                      VkImageSubresourceRange subresource_range,
                                                      VkImageViewCreateFlags flags) {
  auto info = make_empty_image_view_create_info();
  info.image = image;
  info.flags = flags;
  info.viewType = type;
  info.format = format;
  info.components = components;
  info.subresourceRange = subresource_range;
  return info;
}

VkImageCreateInfo vk::make_image_create_info(VkImageType type,
                                             VkFormat format,
                                             VkExtent3D extent,
                                             VkImageUsageFlags usage,
                                             VkImageTiling tiling,
                                             uint32_t array_layers,
                                             uint32_t mip_levels,
                                             VkSampleCountFlagBits samples,
                                             VkSharingMode sharing_mode) {
  auto info = make_empty_image_create_info();
  info.imageType = type;
  info.format = format;
  info.extent = extent;
  info.mipLevels = mip_levels;
  info.arrayLayers = array_layers;
  info.samples = samples;
  info.tiling = tiling;
  info.usage = usage;
  info.sharingMode = sharing_mode;
  return info;
}

VkImageCreateInfo vk::make_2d_image_create_info(VkFormat format,
                                                VkExtent2D extent,
                                                VkImageUsageFlags usage,
                                                VkImageTiling tiling,
                                                uint32_t array_layers,
                                                uint32_t mip_levels,
                                                VkSampleCountFlagBits samples,
                                                VkSharingMode sharing_mode) {
  return make_image_create_info(
    VK_IMAGE_TYPE_2D,
    format,
    VkExtent3D{extent.width, extent.height, 1},
    usage,
    tiling,
    array_layers,
    mip_levels,
    samples,
    sharing_mode);
}

Result<ManagedImage>
vk::create_device_local_image(Allocator* allocator, const VkImageCreateInfo* info) {
  AllocationCreateInfo alloc_info{};
  alloc_info.required_memory_properties = MemoryProperty::DeviceLocal;
  return create_managed_image(allocator, info, &alloc_info);
}

GROVE_NAMESPACE_END
