#pragma once

#include "../vk/vk.hpp"

namespace grove {

namespace gfx {
struct Context;
}

class PostProcessBlitter {
public:
  struct InitInfo {
    gfx::Context& context;
  };

  struct RenderInfo {
    gfx::Context& graphics_context;
    VkDevice device;
    vk::DescriptorSystem& desc_system;
    vk::SamplerSystem& sampler_system;
    VkCommandBuffer cmd;
    VkViewport viewport;
    VkRect2D scissor_rect;
    const vk::SampleImageView& source;
  };

public:
  void initialize(const InitInfo& info);
  void terminate();
  void render_post_process_pass(const RenderInfo& info);
  void render_present_pass(const RenderInfo& info);
};

}