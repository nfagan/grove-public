#include "forward_write_back_pass.hpp"
#include "pass_common.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include "grove/common/scope.hpp"

#define USE_POST_FORWARD_PASS (1)

GROVE_NAMESPACE_BEGIN

using namespace vk;

namespace {

[[maybe_unused]] constexpr const char* logging_id() {
  return "vk/forward_write_back_pass";
}

VkAttachmentReference2 make_attachment_reference2(uint32_t attachment,
                                                  VkImageLayout layout,
                                                  VkImageAspectFlags aspect) {
  VkAttachmentReference2 ref{};
  ref.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
  ref.attachment = attachment;
  ref.layout = layout;
  ref.aspectMask = aspect;
  return ref;
}

Result<RenderPass> create_render_pass(VkInstance instance,
                                      VkDevice device,
                                      VkFormat color_attachment_format,
                                      VkFormat depth_attachment_format,
                                      VkSampleCountFlagBits num_samples,
                                      bool msaa_enabled,
                                      VkResolveModeFlagBits resolve_mode) {
  VkAttachmentDescription2 color_attachment{};
  color_attachment.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
  color_attachment.format = color_attachment_format;
  color_attachment.samples = num_samples;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = msaa_enabled ?
    VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attachment.finalLayout = msaa_enabled ?
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkAttachmentDescription2 depth_stencil_attachment{};
  depth_stencil_attachment.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
  depth_stencil_attachment.format = depth_attachment_format;
  depth_stencil_attachment.samples = num_samples;
  depth_stencil_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth_stencil_attachment.storeOp = msaa_enabled ?
    VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
  depth_stencil_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depth_stencil_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_stencil_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depth_stencil_attachment.finalLayout = msaa_enabled ?
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL :
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

  VkAttachmentDescription2 color_attach_resolve{};
  color_attach_resolve.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
  color_attach_resolve.format = color_attachment_format;
  color_attach_resolve.samples = VK_SAMPLE_COUNT_1_BIT;
  color_attach_resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attach_resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attach_resolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attach_resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attach_resolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attach_resolve.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkAttachmentDescription2 depth_attach_resolve{};
  depth_attach_resolve.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
  depth_attach_resolve.format = depth_attachment_format;
  depth_attach_resolve.samples = VK_SAMPLE_COUNT_1_BIT;
  depth_attach_resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depth_attach_resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depth_attach_resolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depth_attach_resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_attach_resolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depth_attach_resolve.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

  //  Color attachment
  VkAttachmentReference2 color_attachment_ref = make_attachment_reference2(
    0,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    VK_IMAGE_ASPECT_COLOR_BIT);

  //  Depth attachment
  VkAttachmentReference2 depth_attachment_ref = make_attachment_reference2(
    1,
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    VK_IMAGE_ASPECT_DEPTH_BIT);

  //  Color resolve, if multi-sampled
  VkAttachmentReference2 color_attach_resolve_ref = make_attachment_reference2(
    2,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    VK_IMAGE_ASPECT_COLOR_BIT);

  //  Depth resolve, if multi-sampled
  VkAttachmentReference2 depth_attach_resolve_ref = make_attachment_reference2(
    3,
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    VK_IMAGE_ASPECT_DEPTH_BIT);

  VkSubpassDescriptionDepthStencilResolve depth_stencil_resolve{};
  depth_stencil_resolve.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE;
  depth_stencil_resolve.depthResolveMode = resolve_mode;
  depth_stencil_resolve.stencilResolveMode = VK_RESOLVE_MODE_NONE;
  depth_stencil_resolve.pDepthStencilResolveAttachment = &depth_attach_resolve_ref;

  VkSubpassDescription2 subpass_desc{};
  subpass_desc.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
  subpass_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass_desc.colorAttachmentCount = 1;
  subpass_desc.pColorAttachments = &color_attachment_ref;
  subpass_desc.pDepthStencilAttachment = &depth_attachment_ref;
  if (msaa_enabled) {
    //  Color attach resolve
    subpass_desc.pResolveAttachments = &color_attach_resolve_ref;
    //  Depth stencil attach resolve
    subpass_desc.pNext = &depth_stencil_resolve;
  }

  std::array<VkSubpassDependency2, 2> subpass_depends{};
  //  https://github.com/SaschaWillems/Vulkan/blob/master/examples/shadowmapping/shadowmapping.cpp
  subpass_depends[0].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
  subpass_depends[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  subpass_depends[0].dstSubpass = 0;
  subpass_depends[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  subpass_depends[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  subpass_depends[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  subpass_depends[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                                     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  subpass_depends[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  subpass_depends[1].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
  subpass_depends[1].srcSubpass = 0;
  subpass_depends[1].dstSubpass = VK_SUBPASS_EXTERNAL;
#if USE_POST_FORWARD_PASS
  subpass_depends[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  subpass_depends[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                                     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  subpass_depends[1].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  subpass_depends[1].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                                     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
//                                     VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
//                                     VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
  subpass_depends[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
#else
  subpass_depends[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  subpass_depends[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                                     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  subpass_depends[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  subpass_depends[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  subpass_depends[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
#endif

  std::array<VkAttachmentDescription2, 4> attachments = {
    color_attachment,
    depth_stencil_attachment,
    color_attach_resolve,
    depth_attach_resolve
  };

  VkRenderPassCreateInfo2 rp_create_info{};
  rp_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2;
  rp_create_info.attachmentCount = msaa_enabled ? 4 : 2;
  rp_create_info.pAttachments = attachments.data();
  rp_create_info.subpassCount = 1;
  rp_create_info.pSubpasses = &subpass_desc;
  rp_create_info.dependencyCount = uint32_t(subpass_depends.size());
  rp_create_info.pDependencies = subpass_depends.data();

  return vk::create_render_pass2(instance, device, &rp_create_info);
}

Result<Framebuffer> create_framebuffer(VkDevice device,
                                       VkRenderPass render_pass,
                                       VkImageView* attachments,
                                       uint32_t num_attachments,
                                       uint32_t width,
                                       uint32_t height) {
  VkFramebufferCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  create_info.width = width;
  create_info.height = height;
  create_info.layers = 1;
  create_info.attachmentCount = num_attachments;
  create_info.pAttachments = attachments;
  create_info.renderPass = render_pass;
  return grove::create_framebuffer(device, &create_info);
}

} //  anon

void vk::destroy_forward_write_back_pass(ForwardWriteBackPass* pass, VkDevice device) {
  destroy_framebuffer(&pass->framebuffer, device);
  destroy_render_pass(&pass->render_pass, device);
  *pass = {};
}

Result<ForwardWriteBackPass>
vk::create_forward_write_back_pass(const ForwardWriteBackPassCreateInfo* info) {
  ForwardWriteBackPass result;
  bool success = false;
  GROVE_SCOPE_EXIT {
    if (!success) {
      destroy_forward_write_back_pass(&result, info->device);
    }
  };

  {
    auto err = create_attachment_image_and_view(
      info->device,
      info->allocator,
      info->color_format,
      info->image_extent.width,
      info->image_extent.height,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      VK_SAMPLE_COUNT_1_BIT,
      VK_IMAGE_ASPECT_COLOR_BIT,
      &result.single_sample_color_image,
      &result.single_sample_color_image_view);
    if (err) {
      return error_cast<ForwardWriteBackPass>(err);
    }
  }
  {
    auto err = create_attachment_image_and_view(
      info->device,
      info->allocator,
      info->depth_format,
      info->image_extent.width,
      info->image_extent.height,
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      VK_SAMPLE_COUNT_1_BIT,
      VK_IMAGE_ASPECT_DEPTH_BIT,
      &result.single_sample_depth_image,
      &result.single_sample_depth_image_view);
    if (err) {
      return error_cast<ForwardWriteBackPass>(err);
    }
  }

  const bool msaa_enabled = info->image_samples != VK_SAMPLE_COUNT_1_BIT;
  if (msaa_enabled) {
    {
      auto err = create_attachment_image_and_view(
        info->device,
        info->allocator,
        info->color_format,
        info->image_extent.width,
        info->image_extent.height,
        VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        info->image_samples,
        VK_IMAGE_ASPECT_COLOR_BIT,
        &result.multisample_color_image,
        &result.multisample_color_image_view);
      if (err) {
        return error_cast<ForwardWriteBackPass>(err);
      }
    }
    {
      auto err = create_attachment_image_and_view(
        info->device,
        info->allocator,
        info->depth_format,
        info->image_extent.width,
        info->image_extent.height,
        VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        info->image_samples,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        &result.multisample_depth_image,
        &result.multisample_depth_image_view);
      if (err) {
        return error_cast<ForwardWriteBackPass>(err);
      }
    }
  }

  {
    auto res = grove::create_render_pass(
      info->instance,
      info->device,
      info->color_format,
      info->depth_format,
      info->image_samples,
      msaa_enabled,
      info->depth_resolve_mode);
    if (!res) {
      return error_cast<ForwardWriteBackPass>(res);
    } else {
      result.render_pass = std::move(res.value);
    }
  }

  {
    VkImageView attachments[4]{};
    uint32_t num_attachments;
    if (msaa_enabled) {
      attachments[0] = result.multisample_color_image_view.contents().handle;
      attachments[1] = result.multisample_depth_image_view.contents().handle;
      attachments[2] = result.single_sample_color_image_view.contents().handle;
      attachments[3] = result.single_sample_depth_image_view.contents().handle;
      num_attachments = 4;
    } else {
      attachments[0] = result.single_sample_color_image_view.contents().handle;
      attachments[1] = result.single_sample_depth_image_view.contents().handle;
      num_attachments = 2;
    }
    auto res = grove::create_framebuffer(
      info->device,
      result.render_pass.handle,
      attachments,
      num_attachments,
      info->image_extent.width,
      info->image_extent.height);
    if (!res) {
      return error_cast<ForwardWriteBackPass>(res);
    } else {
      result.framebuffer = std::move(res.value);
    }
  }

  result.color_image_format = info->color_format;
  result.depth_image_format = info->depth_format;
  result.image_samples = info->image_samples;
  result.image_extent = info->image_extent;

  success = true;
  return result;
}

vk::SampleImageView vk::ForwardWriteBackPass::make_sample_color_image_view() const {
  vk::SampleImageView result{};
  result.view = single_sample_color_image_view.contents().handle;
  result.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  return result;
}

vk::SampleImageView vk::ForwardWriteBackPass::make_sample_depth_image_view() const {
  vk::SampleImageView result{};
  result.view = single_sample_depth_image_view.contents().handle;
  result.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  return result;
}

VkImage vk::ForwardWriteBackPass::get_single_sample_depth_image() const {
  assert(single_sample_depth_image.is_valid());
  return single_sample_depth_image.contents().image.handle;
}

size_t vk::ForwardWriteBackPass::approx_image_memory_usage() const {
  size_t res{};
  if (multisample_color_image.is_valid()) {
    res += multisample_color_image.get_allocation_size();
  }
  if (multisample_depth_image.is_valid()) {
    res += multisample_depth_image.get_allocation_size();
  }
  if (single_sample_color_image.is_valid()) {
    res += single_sample_color_image.get_allocation_size();
  }
  if (single_sample_depth_image.is_valid()) {
    res += single_sample_depth_image.get_allocation_size();
  }
  return res;
}

Optional<VkResolveModeFlagBits>
vk::choose_forward_write_back_pass_depth_resolve_mode(const PhysicalDevice& device) {
  auto props = device.get_depth_stencil_resolve_properties();
  VkResolveModeFlagBits desired = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
  if ((props.supportedDepthResolveModes & desired) == desired) {
    return Optional<VkResolveModeFlagBits>(desired);
  } else {
    return NullOpt{};
  }
}

VkSampleCountFlagBits vk::choose_forward_write_back_pass_samples(const PhysicalDevice& device,
                                                                 int num_samples) {
  if (num_samples > 0) {
    auto sample_flag_bits = device.framebuffer_color_depth_sample_count_flag_bits(num_samples);
    if (sample_flag_bits) {
      return sample_flag_bits.value();
    } else {
      GROVE_LOG_WARNING_CAPTURE_META(
        "Desired forward pass sample counts not supported.", logging_id());
    }
  }
  return VK_SAMPLE_COUNT_1_BIT;
}

Optional<VkFormat> vk::choose_forward_write_back_pass_depth_format(VkPhysicalDevice device) {
  const VkFormat acceptable_formats[2] = {
    VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT
  };

  auto depth_format_res = vk::select_format_with_features(
    device,
    acceptable_formats,
    2,
    VK_IMAGE_TILING_OPTIMAL,
    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);

  if (depth_format_res) {
    return Optional<VkFormat>(depth_format_res.value);
  } else {
    return NullOpt{};
  }
}

GROVE_NAMESPACE_END