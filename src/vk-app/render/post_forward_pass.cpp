#include "post_forward_pass.hpp"
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
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  color_attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkAttachmentDescription depth_stencil_attachment{};
  depth_stencil_attachment.format = depth_attachment_format;
  depth_stencil_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depth_stencil_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  depth_stencil_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depth_stencil_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depth_stencil_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_stencil_attachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  depth_stencil_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

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

  uint32_t num_subpass_depends{2};
  VkSubpassDependency subpass_dependencies[2];

  subpass_dependencies[0] = {};
  subpass_dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  subpass_dependencies[0].dstSubpass = 0;
  subpass_dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                         VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  subpass_dependencies[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                                          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  subpass_dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                         VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  subpass_dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                                          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                          VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

  subpass_dependencies[1] = {};
  subpass_dependencies[1].srcSubpass = 0;
  subpass_dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  subpass_dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  subpass_dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                                          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  subpass_dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  subpass_dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  subpass_dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

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

} //  anon

void vk::destroy_post_forward_pass(PostForwardPass* pass, VkDevice device) {
  destroy_framebuffer(&pass->framebuffer, device);
  destroy_render_pass(&pass->render_pass, device);
  *pass = {};
}

Result<PostForwardPass> vk::create_post_forward_pass(const PostForwardPassCreateInfo* info) {
  PostForwardPass result{};
  bool success{};
  GROVE_SCOPE_EXIT {
    if (!success) {
      vk::destroy_post_forward_pass(&result, info->device);
    }
  };

  auto pass_res = do_create_render_pass(info->device, info->color_format, info->depth_format);
  if (!pass_res) {
    return error_cast<PostForwardPass>(pass_res);
  } else {
    result.render_pass = std::move(pass_res.value);
  }

  const VkImageView* color_im = &info->single_sample_color_image_view;
  const VkImageView* depth_im = &info->single_sample_depth_image_view;

  auto err = create_framebuffers_with_one_color_attachment(
    info->device, color_im, 1, *depth_im, info->image_extent, result.render_pass.handle,
    &result.framebuffer);
  if (err) {
    return error_cast<PostForwardPass>(err);
  }

  success = true;
  return result;
}

GROVE_NAMESPACE_END
