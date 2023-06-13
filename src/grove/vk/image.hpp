#pragma once

#include "common.hpp"
#include "memory.hpp"
#include "grove/common/Unique.hpp"
#include <memory>

namespace grove::vk {

struct Image {
  VkImage handle{VK_NULL_HANDLE};
  VkExtent3D extent{};
};

struct ImageView {
  VkImageView handle{VK_NULL_HANDLE};
};

struct SampleImageView {
  VkImageView view;
  VkImageLayout layout;
};

class ManagedImageView {
public:
  struct Contents {
    VkImageView handle;
  };
public:
  GROVE_NONCOPYABLE(ManagedImageView)
  ManagedImageView() = default;
  explicit ManagedImageView(ImageView view, VkDevice device);
  ManagedImageView(ManagedImageView&& other) noexcept = default;
  ManagedImageView& operator=(ManagedImageView&& other) noexcept = default;

  Contents contents() const;
  bool is_valid() const;
  void destroy();
private:
  Unique<ImageView> view;
};

class ManagedImage {
public:
  struct Contents {
    Image image{};
  };

public:
  GROVE_NONCOPYABLE(ManagedImage)
  ManagedImage() = default;
  ManagedImage(Allocator* allocator, AllocationRecordHandle allocation, Image image);
  ManagedImage(ManagedImage&& other) noexcept;
  ManagedImage& operator=(ManagedImage&& other) noexcept;
  ~ManagedImage();

  Contents contents() const;
  bool is_valid() const;
  void destroy();
  size_t get_allocation_size() const;

  friend inline void swap(ManagedImage& a, ManagedImage& b) {
    std::swap(a.allocator, b.allocator);
    std::swap(a.allocation, b.allocation);
    std::swap(a.image, b.image);
  }
private:
  Allocator* allocator{};
  AllocationRecordHandle allocation{};
  Image image{};
};

Result<Image> create_image(VkDevice device, const VkImageCreateInfo* info);
void destroy_image(Image* image, VkDevice device);

Result<ImageView> create_image_view(VkDevice device, const VkImageViewCreateInfo* info);
void destroy_image_view(ImageView* view, VkDevice device);

Result<ManagedImage> create_managed_image(Allocator* allocator,
                                          const VkImageCreateInfo* create_info,
                                          const AllocationCreateInfo* alloc_info);

inline VkImageCreateInfo make_empty_image_create_info() {
  VkImageCreateInfo result{};
  result.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  return result;
}

inline VkImageViewCreateInfo make_empty_image_view_create_info() {
  VkImageViewCreateInfo result{};
  result.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  return result;
}

inline VkComponentMapping make_identity_component_mapping() {
  VkComponentMapping mapping{};
  return mapping;
}

inline VkImageMemoryBarrier make_empty_image_memory_barrier() {
  VkImageMemoryBarrier result{};
  result.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  return result;
}

inline VkImageViewCreateInfo make_2d_image_view_create_info(VkImage image,
                                                            VkFormat format,
                                                            VkImageAspectFlags aspect) {
  auto info = make_empty_image_view_create_info();
  info.subresourceRange.layerCount = 1;
  info.subresourceRange.baseArrayLayer = 0;
  info.subresourceRange.levelCount = 1;
  info.subresourceRange.baseMipLevel = 0;
  info.subresourceRange.aspectMask = aspect;
  info.components = make_identity_component_mapping();
  info.image = image;
  info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  info.format = format;
  return info;
}

inline VkImageViewCreateInfo make_2d_image_array_view_create_info(VkImage image,
                                                                  VkFormat format,
                                                                  VkImageAspectFlags aspect,
                                                                  uint32_t base_layer,
                                                                  uint32_t layer_count) {
  auto info = make_empty_image_view_create_info();
  info.subresourceRange.layerCount = layer_count;
  info.subresourceRange.baseArrayLayer = base_layer;
  info.subresourceRange.levelCount = 1;
  info.subresourceRange.baseMipLevel = 0;
  info.subresourceRange.aspectMask = aspect;
  info.components = make_identity_component_mapping();
  info.image = image;
  info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
  info.format = format;
  return info;
}

inline VkImageSubresourceRange make_image_subresource_range(VkImageAspectFlags aspect_mask,
                                                            uint32_t layer,
                                                            uint32_t num_layers,
                                                            uint32_t mip,
                                                            uint32_t num_mips) {
  VkImageSubresourceRange result;
  result.aspectMask = aspect_mask;
  result.baseMipLevel = mip;
  result.levelCount = num_mips;
  result.baseArrayLayer = layer;
  result.layerCount = num_layers;
  return result;
}

}