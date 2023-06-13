#include "image.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

using namespace vk;

namespace {

Image make_image(VkImage handle, VkExtent3D extent) {
  Image result{};
  result.handle = handle;
  result.extent = extent;
  return result;
}

} //  anon

Result<Image> vk::create_image(VkDevice device, const VkImageCreateInfo* info) {
  VkImage handle;
  auto res = vkCreateImage(device, info, GROVE_VK_ALLOC, &handle);
  if (res != VK_SUCCESS) {
    return {res, "Failed to create image."};
  } else {
    return make_image(handle, info->extent);
  }
}

void vk::destroy_image(Image* image, VkDevice device) {
  if (device != VK_NULL_HANDLE) {
    vkDestroyImage(device, image->handle, GROVE_VK_ALLOC);
    image->handle = VK_NULL_HANDLE;
    image->extent = {};
  } else {
    GROVE_ASSERT(image->handle == VK_NULL_HANDLE);
  }
}

Result<ImageView> vk::create_image_view(VkDevice device, const VkImageViewCreateInfo* create_info) {
  VkImageView handle{};
  auto res = vkCreateImageView(device, create_info, GROVE_VK_ALLOC, &handle);
  if (res != VK_SUCCESS) {
    return {res, "Failed to create image view."};
  } else {
    vk::ImageView result{};
    result.handle = handle;
    return result;
  }
}

void vk::destroy_image_view(ImageView* view, VkDevice device) {
  if (device != VK_NULL_HANDLE) {
    vkDestroyImageView(device, view->handle, GROVE_VK_ALLOC);
    view->handle = VK_NULL_HANDLE;
  } else {
    GROVE_ASSERT(view->handle == VK_NULL_HANDLE);
  }
}

Result<ManagedImage> vk::create_managed_image(Allocator* allocator,
                                              const VkImageCreateInfo* create_info,
                                              const AllocationCreateInfo* alloc_info) {
  VkImage im_handle;
  AllocationRecordHandle alloc;
  if (auto err = allocator->create_image(create_info, alloc_info, &im_handle, &alloc)) {
    return error_cast<vk::ManagedImage>(err);
  } else {
    return ManagedImage{allocator, alloc, make_image(im_handle, create_info->extent)};
  }
}

vk::ManagedImageView::ManagedImageView(ImageView image_view, VkDevice device) :
  view{
    std::move(image_view),
    [device](ImageView* v) { destroy_image_view(v, device); }
  } {
  //
}

void vk::ManagedImageView::destroy() {
  view = {};
}

bool vk::ManagedImageView::is_valid() const {
  return view.get().handle != VK_NULL_HANDLE;
}

vk::ManagedImageView::Contents vk::ManagedImageView::contents() const {
  return {view.get().handle};
}

vk::ManagedImage::ManagedImage(Allocator* allocator,
                               AllocationRecordHandle allocation,
                               Image image) :
  allocator{allocator},
  allocation{allocation},
  image{image} {
  //
}

vk::ManagedImage::ManagedImage(ManagedImage&& other) noexcept {
  swap(*this, other);
}

vk::ManagedImage& vk::ManagedImage::operator=(ManagedImage&& other) noexcept {
  ManagedImage tmp{std::move(other)};
  swap(*this, tmp);
  return *this;
}

vk::ManagedImage::~ManagedImage() {
  if (allocator) {
    destroy();
  } else {
    GROVE_ASSERT(image.handle == VK_NULL_HANDLE);
  }
}

vk::ManagedImage::Contents vk::ManagedImage::contents() const {
  return {image};
}

size_t vk::ManagedImage::get_allocation_size() const {
  if (allocator && allocation != null_allocation_record_handle()) {
    return allocator->get_size(allocation);
  } else {
    GROVE_ASSERT(false);
    return 0;
  }
}

void vk::ManagedImage::destroy() {
  GROVE_ASSERT(allocator && is_valid());
  allocator->destroy_image(image.handle, allocation);
  allocator = nullptr;
  allocation = null_allocation_record_handle();
  image = {};
}

bool vk::ManagedImage::is_valid() const {
  return image.handle != VK_NULL_HANDLE;
}

GROVE_NAMESPACE_END
