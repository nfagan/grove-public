#include "shadow_pass.hpp"
#include "grove/common/common.hpp"
#include "grove/common/scope.hpp"

GROVE_NAMESPACE_BEGIN

using namespace vk;

namespace {

Result<RenderPass> create_shadow_render_pass(VkDevice device,
                                             VkFormat depth_attachment_format) {
  VkAttachmentDescription depth_stencil_attachment{};
  depth_stencil_attachment.format = depth_attachment_format;
  depth_stencil_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depth_stencil_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth_stencil_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depth_stencil_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depth_stencil_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_stencil_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depth_stencil_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

  VkAttachmentReference depth_attachment_ref{};
  depth_attachment_ref.attachment = 0;
  depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 0;
  subpass.pDepthStencilAttachment = &depth_attachment_ref;

  std::array<VkSubpassDependency, 2> subpass_depends{};
  //  https://github.com/SaschaWillems/Vulkan/blob/master/examples/shadowmapping/shadowmapping.cpp
  subpass_depends[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  subpass_depends[0].dstSubpass = 0;
  subpass_depends[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  subpass_depends[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  subpass_depends[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  subpass_depends[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  subpass_depends[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  subpass_depends[1].srcSubpass = 0;
  subpass_depends[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  subpass_depends[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  subpass_depends[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  subpass_depends[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  subpass_depends[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  subpass_depends[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  const VkAttachmentDescription attachments[] = {
    depth_stencil_attachment
  };

  auto create_info = make_empty_render_pass_create_info();
  create_info.attachmentCount = 1;
  create_info.pAttachments = attachments;
  create_info.subpassCount = 1;
  create_info.pSubpasses = &subpass;
  create_info.dependencyCount = 2;
  create_info.pDependencies = subpass_depends.data();

  return create_render_pass(device, &create_info);
}

} //  anon

size_t vk::ShadowPass::approx_image_memory_usage() const {
  size_t res{};
  if (image.is_valid()) {
    res += image.get_allocation_size();
  }
  return res;
}

Optional<VkFormat> vk::choose_shadow_pass_image_format(const PhysicalDevice& device) {
  const VkFormat acceptable_depth_formats[2] = {
    VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT
  };
  auto depth_format_res = vk::select_format_with_features(
    device.handle,
    acceptable_depth_formats,
    2,
    VK_IMAGE_TILING_OPTIMAL,
    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
  if (depth_format_res) {
    return Optional<VkFormat>(depth_format_res.value);
  } else {
    return NullOpt{};
  }
}

void vk::destroy_shadow_pass(ShadowPass* pass, VkDevice device) {
  for (auto& fb : pass->framebuffers) {
    vk::destroy_framebuffer(&fb, device);
  }
  for (auto& view : pass->sub_views) {
    vk::destroy_image_view(&view, device);
  }
  vk::destroy_image_view(&pass->array_view, device);
  vk::destroy_render_pass(&pass->render_pass, device);
  *pass = {};
}

Result<ShadowPass> vk::create_shadow_pass(const CreateShadowPassInfo* info) {
  GROVE_ASSERT(info->num_layers > 0 && info->image_dim > 0 && info->samples != 0);

  ShadowPass result;
  bool success = false;
  GROVE_SCOPE_EXIT {
    if (!success) {
      vk::destroy_shadow_pass(&result, info->device);
    }
  };

  {
    //  render pass
    auto rp_res = create_shadow_render_pass(info->device, info->depth_format);
    if (!rp_res) {
      return error_cast<ShadowPass>(rp_res);
    } else {
      result.render_pass = std::move(rp_res.value);
    }
  }

  const VkExtent3D extent{info->image_dim, info->image_dim, 1};
  const uint32_t num_layers = info->num_layers;
  const VkSampleCountFlagBits raster_samples = info->samples;
  const VkFormat depth_format = info->depth_format;

  {
    //  shadow image
    auto im_create_info = make_image_create_info(
      VK_IMAGE_TYPE_2D,
      depth_format,
      extent,
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      VK_IMAGE_TILING_OPTIMAL,
      num_layers,
      1,
      raster_samples,
      VK_SHARING_MODE_EXCLUSIVE);

    auto im_res = create_device_local_image(info->allocator, &im_create_info);
    if (!im_res) {
      return error_cast<ShadowPass>(im_res);
    }

    result.image = std::move(im_res.value);
    result.format = depth_format;
    result.extent.width = extent.width;
    result.extent.height = extent.height;
    result.raster_samples = raster_samples;
  }

  {
    auto view_create_info = vk::make_2d_image_array_view_create_info(
      result.image.contents().image.handle,
      depth_format,
      VK_IMAGE_ASPECT_DEPTH_BIT,
      0,
      num_layers);
    auto view_res = vk::create_image_view(info->device, &view_create_info);
    if (!view_res) {
      return error_cast<ShadowPass>(view_res);
    }
    result.array_view = std::move(view_res.value);
  }

  for (uint32_t i = 0; i < num_layers; i++) {
    auto view_create_info = vk::make_2d_image_array_view_create_info(
      result.image.contents().image.handle,
      depth_format,
      VK_IMAGE_ASPECT_DEPTH_BIT,
      i,
      1);
    auto view_res = vk::create_image_view(info->device, &view_create_info);
    if (!view_res) {
      return error_cast<ShadowPass>(view_res);
    }

    result.sub_views.push_back(std::move(view_res.value));
    auto& view = result.sub_views.back();

    VkFramebufferCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    create_info.renderPass = result.render_pass.handle;
    create_info.attachmentCount = 1;
    create_info.pAttachments = &view.handle;
    create_info.width = result.extent.width;
    create_info.height = result.extent.height;
    create_info.layers = 1;
    auto fb_res = vk::create_framebuffer(info->device, &create_info);
    if (!fb_res) {
      return error_cast<ShadowPass>(fb_res);
    }

    result.framebuffers.push_back(std::move(fb_res.value));
  }

  success = true;
  return result;
}

GROVE_NAMESPACE_END
