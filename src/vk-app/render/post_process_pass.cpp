#include "post_process_pass.hpp"
#include "pass_common.hpp"
#include "grove/common/common.hpp"
#include "grove/common/scope.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace vk;

Result<RenderPass> do_create_render_pass(
  VkDevice device, VkFormat color_attachment_format, VkFormat depth_attachment_format,
  bool transition_to_present) {
  //
  VkAttachmentDescription color_attachment{};
  color_attachment.format = color_attachment_format;
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attachment.finalLayout = transition_to_present ?
    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

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

  uint32_t num_subpass_depends{1};
  VkSubpassDependency subpass_dependencies[2];

  subpass_dependencies[0] = {};
  subpass_dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  subpass_dependencies[0].dstSubpass = 0;
  subpass_dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  subpass_dependencies[0].srcAccessMask = 0;
  subpass_dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  subpass_dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  if (!transition_to_present) {
    subpass_dependencies[1] = {};
    subpass_dependencies[1].srcSubpass = 0;
    subpass_dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    subpass_dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpass_dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    subpass_dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpass_dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    subpass_dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    num_subpass_depends = 2;
  }

  const VkAttachmentDescription attachments[2] = {
    color_attachment,
    depth_stencil_attachment,
  };

  auto create_info = make_empty_render_pass_create_info();
  create_info.attachmentCount = 2;
  create_info.pAttachments = attachments;
  create_info.subpassCount = 1;
  create_info.pSubpasses = &subpass;
  create_info.dependencyCount = num_subpass_depends;
  create_info.pDependencies = subpass_dependencies;

  return create_render_pass(device, &create_info);
}

Result<std::vector<Framebuffer>> create_framebuffers(
  VkDevice device, const VkImageView* image_views, uint32_t num_image_views,
  const ManagedImageView& depth_image, VkExtent2D extent, VkRenderPass render_pass) {
  //
  std::vector<Framebuffer> result(num_image_views);
  auto err = create_framebuffers_with_one_color_attachment(
    device, image_views, num_image_views, depth_image.contents().handle,
    extent, render_pass, result.data());
  if (err) {
    return error_cast<std::vector<Framebuffer>>(err);
  } else {
    return result;
  }
}

} //  anon

size_t vk::PostProcessPass::approx_image_memory_usage() const {
  size_t res{};
  if (depth_image.is_valid()) {
    res += depth_image.get_allocation_size();
  }
  if (maybe_color_image.is_valid()) {
    res += maybe_color_image.get_allocation_size();
  }
  return res;
}

vk::SampleImageView vk::PostProcessPass::make_sample_color_image_view() const {
  assert(maybe_color_image.is_valid() && maybe_color_image_view.is_valid());
  vk::SampleImageView result{};
  result.view = maybe_color_image_view.contents().handle;
  result.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  return result;
}

void vk::destroy_post_process_pass(PostProcessPass* pass, VkDevice device) {
  for (auto& fb : pass->framebuffers) {
    vk::destroy_framebuffer(&fb, device);
  }
  vk::destroy_render_pass(&pass->render_pass, device);
  *pass = {};
}

Result<PostProcessPass> vk::create_post_process_pass(const PostProcessPassCreateInfo* info) {
  if (info->separate_present_pass_enabled) {
    assert(!info->present_image_views && info->num_present_image_views == 0);
  } else {
    assert(info->present_image_views && info->num_present_image_views > 0);
  }

  PostProcessPass result;
  bool success{};
  GROVE_SCOPE_EXIT {
    if (!success) {
      vk::destroy_post_process_pass(&result, info->device);
    }
  };

  {
    auto err = create_depth_image_components(
      info->device, info->allocator, info->depth_format, info->image_extent,
      &result.depth_image, &result.depth_image_view);
    if (err) {
      return error_cast<PostProcessPass>(err);
    }
  }

  {
    const bool transition_to_present = !info->separate_present_pass_enabled;
    auto res = do_create_render_pass(
      info->device, info->color_format, info->depth_format, transition_to_present);
    if (!res) {
      return error_cast<PostProcessPass>(res);
    } else {
      result.render_pass = std::move(res.value);
    }
  }

  if (info->separate_present_pass_enabled) {
    //  Create color images to render into
    {
      auto err = create_attachment_image_and_view(
        info->device, info->allocator, info->color_format,
        info->image_extent.width, info->image_extent.height,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        &result.maybe_color_image,
        &result.maybe_color_image_view);
      if (err) {
        return error_cast<PostProcessPass>(err);
      }
    }

    VkImageView view = result.maybe_color_image_view.contents().handle;
    auto fb_res = create_framebuffers(
      info->device, &view, 1, result.depth_image_view, info->image_extent, result.render_pass.handle);
    if (!fb_res) {
      return error_cast<PostProcessPass>(fb_res);
    } else {
      result.framebuffers = std::move(fb_res.value);
    }

  } else {
    auto res = create_framebuffers(
      info->device, info->present_image_views, info->num_present_image_views,
      result.depth_image_view, info->image_extent, result.render_pass.handle);
    if (!res) {
      return error_cast<PostProcessPass>(res);
    } else {
      result.framebuffers = std::move(res.value);
    }
  }

  result.color_image_format = info->color_format;
  result.depth_image_format = info->depth_format;
  result.raster_samples = VK_SAMPLE_COUNT_1_BIT;
  result.image_extent = info->image_extent;

  success = true;
  return result;
}

Optional<VkFormat> vk::choose_post_process_pass_depth_format(VkPhysicalDevice device) {
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

GROVE_NAMESPACE_END
