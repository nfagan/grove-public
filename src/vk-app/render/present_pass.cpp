#include "present_pass.hpp"
#include "pass_common.hpp"
#include "grove/common/common.hpp"
#include "grove/common/scope.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace vk;

Result<RenderPass> do_create_render_pass(
  VkDevice device, VkFormat color_attachment_format, VkFormat depth_attachment_format) {
  //
  VkAttachmentDescription color_attachment{};
  color_attachment.format = color_attachment_format;
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentDescription depth_stencil_attachment{};
  depth_stencil_attachment.format = depth_attachment_format;
  depth_stencil_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depth_stencil_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth_stencil_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depth_stencil_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depth_stencil_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_stencil_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depth_stencil_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference color_attachment_ref{};
  color_attachment_ref.attachment = 0;
  color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depth_attachment_ref{};
  depth_attachment_ref.attachment = 1;
  depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_attachment_ref;
  subpass.pDepthStencilAttachment = &depth_attachment_ref;
  subpass.pResolveAttachments = nullptr;

  VkSubpassDependency subpass_dependency{};
  subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  subpass_dependency.dstSubpass = 0;
  subpass_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  subpass_dependency.srcAccessMask = 0;
  subpass_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  subpass_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                     VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  const VkAttachmentDescription attachments[2] = {
    color_attachment,
    depth_stencil_attachment,
  };

  auto create_info = make_empty_render_pass_create_info();
  create_info.attachmentCount = 2;
  create_info.pAttachments = attachments;
  create_info.subpassCount = 1;
  create_info.pSubpasses = &subpass;
  create_info.dependencyCount = 1;
  create_info.pDependencies = &subpass_dependency;

  return create_render_pass(device, &create_info);
}

Result<std::vector<Framebuffer>> create_framebuffers(
  VkDevice device, const VkImageView* present_views, uint32_t num_present_views,
  const ManagedImageView& depth_image, VkExtent2D extent, VkRenderPass render_pass) {
  //
  std::vector<Framebuffer> result(num_present_views);
  auto err = create_framebuffers_with_one_color_attachment(
    device, present_views, num_present_views, depth_image.contents().handle,
    extent, render_pass, result.data());
  if (err) {
    return error_cast<std::vector<Framebuffer>>(err);
  } else {
    return result;
  }
}

} //  anon

Result<PresentPass> vk::create_present_pass(const PresentPassCreateInfo* info) {
  PresentPass result{};
  bool success{};
  GROVE_SCOPE_EXIT {
    if (!success) {
      vk::destroy_present_pass(&result, info->device);
    }
  };

  {
    auto err = create_depth_image_components(
      info->device, info->allocator, info->depth_format, info->image_extent,
      &result.depth_image, &result.depth_image_view);
    if (err) {
      return error_cast<PresentPass>(err);
    }
  }

  {
    auto res = do_create_render_pass(info->device, info->color_format, info->depth_format);
    if (!res) {
      return error_cast<PresentPass>(res);
    } else {
      result.render_pass = std::move(res.value);
    }
  }

  {
    auto res = create_framebuffers(
      info->device, info->present_image_views, info->num_present_image_views,
      result.depth_image_view, info->image_extent, result.render_pass.handle);
    if (!res) {
      return error_cast<PresentPass>(res);
    } else {
      result.framebuffers = std::move(res.value);
    }
  }

  result.color_image_format = info->color_format;
  result.depth_image_format = info->depth_format;
  result.raster_samples = VK_SAMPLE_COUNT_1_BIT;

  success = true;
  return result;
}

void vk::destroy_present_pass(PresentPass* pass, VkDevice device) {
  for (auto& fb : pass->framebuffers) {
    vk::destroy_framebuffer(&fb, device);
  }
  vk::destroy_render_pass(&pass->render_pass, device);
  *pass = {};
}

Optional<VkFormat> vk::choose_present_pass_depth_format(VkPhysicalDevice device) {
  const VkFormat acceptable_formats[2] = {
    VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT
  };

  auto depth_format_res = vk::select_format_with_features(
    device,
    acceptable_formats,
    2,
    VK_IMAGE_TILING_OPTIMAL,
    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

  if (depth_format_res) {
    return Optional<VkFormat>(depth_format_res.value);
  } else {
    return NullOpt{};
  }
}

size_t vk::PresentPass::approx_image_memory_usage() const {
  size_t res{};
  if (depth_image.is_valid()) {
    res += depth_image.get_allocation_size();
  }
  return res;
}

GROVE_NAMESPACE_END
