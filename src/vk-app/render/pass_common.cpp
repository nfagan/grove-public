#include "pass_common.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace vk;

Result<ManagedImage> create_image(
  Allocator* allocator, VkFormat format, uint32_t width, uint32_t height,
  VkImageUsageFlags usage, VkSampleCountFlagBits samples) {
  //
  auto create_info = make_image_create_info(
    VK_IMAGE_TYPE_2D,
    format,
    VkExtent3D{width, height, 1},
    usage,
    VK_IMAGE_TILING_OPTIMAL,
    1,
    1,
    samples,
    VK_SHARING_MODE_EXCLUSIVE);
  return create_device_local_image(allocator, &create_info);
}

Result<ManagedImageView> create_image_view(
  VkDevice device, VkFormat format, VkImageAspectFlags aspect, VkImage image) {
  //
  auto create_info = make_image_view_create_info(
    image,
    VK_IMAGE_VIEW_TYPE_2D,
    format,
    make_identity_component_mapping(),
    make_image_subresource_range(aspect, 0, 1, 0, 1));
  auto view_res = vk::create_image_view(device, &create_info);
  if (!view_res) {
    return error_cast<ManagedImageView>(view_res);
  } else {
    return ManagedImageView{std::move(view_res.value), device};
  }
}

} //  anon

Error vk::create_depth_image_components(
  VkDevice device, Allocator* allocator, VkFormat format, VkExtent2D extent,
  ManagedImage* out_image, ManagedImageView* out_view) {
  //
  auto im_create_info = make_image_create_info(
    VK_IMAGE_TYPE_2D,
    format,
    {extent.width, extent.height, 1},
    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    VK_IMAGE_TILING_OPTIMAL, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_SHARING_MODE_EXCLUSIVE);
  auto image_res = create_device_local_image(allocator, &im_create_info);
  if (!image_res) {
    return {image_res.status, image_res.message};
  }

  auto view_create_info = make_image_view_create_info(
    image_res.value.contents().image.handle,
    VK_IMAGE_VIEW_TYPE_2D,
    format,
    make_identity_component_mapping(),
    make_image_subresource_range(VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1));
  auto view_res = create_image_view(device, &view_create_info);
  if (!view_res) {
    return {view_res.status, view_res.message};
  }

  *out_image = std::move(image_res.value);
  *out_view = ManagedImageView{view_res.value, device};
  return {};
}

Error vk::create_framebuffers_with_one_color_attachment(
  VkDevice device, const VkImageView* color_views, uint32_t num_color_views,
  VkImageView depth_image, VkExtent2D extent, VkRenderPass render_pass,
  vk::Framebuffer* out_framebuffers) {
  //
  for (uint32_t i = 0; i < num_color_views; i++) {
    std::array<VkImageView, 2> attachments{{
      color_views[i],
      depth_image
    }};

    VkFramebufferCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    create_info.renderPass = render_pass;
    create_info.attachmentCount = 2;
    create_info.pAttachments = attachments.data();
    create_info.width = extent.width;
    create_info.height = extent.height;
    create_info.layers = 1;

    auto fb_res = create_framebuffer(device, &create_info);
    if (fb_res) {
      out_framebuffers[i] = std::move(fb_res.value);
    } else {
      for (uint32_t j = 0; j < i; j++) {
        destroy_framebuffer(&out_framebuffers[j], device);
      }
      return Error{fb_res.status, fb_res.message};
    }
  }
  return {};
}

Error vk::create_attachment_image_and_view(
  VkDevice device, Allocator* allocator, VkFormat format, uint32_t width, uint32_t height,
  VkImageUsageFlags usage, VkSampleCountFlagBits samples, VkImageAspectFlags aspect,
  ManagedImage* out_image, ManagedImageView* out_view) {
  //
  auto im_res = grove::create_image(allocator, format, width, height, usage, samples);
  if (!im_res) {
    return {im_res.status, im_res.message};
  }
  auto view_res = grove::create_image_view(
    device, format, aspect, im_res.value.contents().image.handle);
  if (!view_res) {
    return {view_res.status, view_res.message};
  }
  *out_image = std::move(im_res.value);
  *out_view = std::move(view_res.value);
  return {};
}

GROVE_NAMESPACE_END
