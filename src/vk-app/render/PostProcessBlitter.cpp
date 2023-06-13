#include "PostProcessBlitter.hpp"
#include "graphics.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace vk;

struct {
  gfx::PipelineHandle nearest_pipeline;
  gfx::PipelineHandle linear_pipeline;
} globals;

} //  anon

void PostProcessBlitter::initialize(const InitInfo& info) {
  {
    auto get_source = []() {
      glsl::LoadVertFragProgramSourceParams params{};
      params.vert_file = "post-process/blit.vert";
      params.frag_file = "post-process/blit.frag";
      return glsl::make_vert_frag_program_source(params);
    };

    auto pass = gfx::get_post_process_pass_handle(&info.context);
    if (auto source = get_source(); source && pass) {
      gfx::GraphicsPipelineCreateInfo create_info{};
      create_info.disable_depth_write = true;
      create_info.disable_depth_test = true;
      create_info.num_color_attachments = 1;
      auto pipe_res = gfx::create_pipeline(
        &info.context, std::move(source.value()), create_info, pass.value());
      if (pipe_res) {
        globals.nearest_pipeline = std::move(pipe_res.value());
      }
    }
  }
  {
    auto get_source = []() {
      glsl::LoadVertFragProgramSourceParams params{};
      params.vert_file = "post-process/blit.vert";
      params.frag_file = "post-process/blit.frag";
      params.compile.vert_defines.push_back(glsl::make_define("SAMPLE_LINEAR"));
      params.compile.frag_defines.push_back(glsl::make_define("SAMPLE_LINEAR"));
      return glsl::make_vert_frag_program_source(params);
    };

    auto pass = gfx::get_post_process_pass_handle(&info.context);
    if (auto source = get_source(); source && pass) {
      gfx::GraphicsPipelineCreateInfo create_info{};
      create_info.disable_depth_write = true;
      create_info.disable_depth_test = true;
      create_info.num_color_attachments = 1;
      auto pipe_res = gfx::create_pipeline(
        &info.context, std::move(source.value()), create_info, pass.value());
      if (pipe_res) {
        globals.linear_pipeline = std::move(pipe_res.value());
      }
    }
  }
}

void PostProcessBlitter::terminate() {
  globals.linear_pipeline = {};
  globals.nearest_pipeline = {};
}

void PostProcessBlitter::render_post_process_pass(const RenderInfo& info) {
  const auto& pd = globals.nearest_pipeline;
  if (!pd.is_valid()) {
    return;
  }

  auto sampler = info.sampler_system.require_simple(
    info.device,
    VK_FILTER_NEAREST,
    VK_FILTER_NEAREST,
    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

  DescriptorSetScaffold set0_scaffold;
  set0_scaffold.set = 0;
  uint32_t set0_bind{};
  push_combined_image_sampler(set0_scaffold, set0_bind++, info.source, sampler);

  auto desc_set0 = gfx::require_updated_descriptor_set(&info.graphics_context, set0_scaffold, pd);
  if (!desc_set0) {
    return;
  }

  cmd::bind_graphics_pipeline(info.cmd, pd.get());
  cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor_rect);
  cmd::bind_graphics_descriptor_sets(info.cmd, pd.get_layout(), 0, 1, &desc_set0.value());
  vkCmdDraw(info.cmd, 3, 1, 0, 0);
}

void PostProcessBlitter::render_present_pass(const RenderInfo& info) {
  const auto& pd = globals.linear_pipeline;
  if (!pd.is_valid()) {
    return;
  }

  auto sampler = info.sampler_system.require_linear_edge_clamp(info.device);
//  auto sampler = info.sampler_system.require_nearest_edge_clamp(info.device);

  DescriptorSetScaffold set0_scaffold;
  set0_scaffold.set = 0;
  uint32_t set0_bind{};
  push_combined_image_sampler(set0_scaffold, set0_bind++, info.source, sampler);

  auto desc_set0 = gfx::require_updated_descriptor_set(&info.graphics_context, set0_scaffold, pd);
  if (!desc_set0) {
    return;
  }

  cmd::bind_graphics_pipeline(info.cmd, pd.get());
  cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor_rect);
  cmd::bind_graphics_descriptor_sets(info.cmd, pd.get_layout(), 0, 1, &desc_set0.value());
  vkCmdDraw(info.cmd, 3, 1, 0, 0);
}

GROVE_NAMESPACE_END
