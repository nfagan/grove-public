#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

namespace grove::vk {
class SampledImageManager;
}

namespace grove::gfx {
struct Context;
}

namespace grove::gui {

struct RenderData;

struct RenderGUIStats {
  uint32_t num_quad_vertices;
  uint32_t num_glyph_quad_vertices;
};

struct RenderGUIBeginFrameInfo {
  uint32_t frame_index;
  gfx::Context* context;
  gui::RenderData* render_data;
  vk::SampledImageManager& sampled_image_manager;
};

struct RenderGUIRenderInfo {
  VkCommandBuffer cmd;
  VkViewport viewport;
  VkRect2D scissor;
  uint32_t frame_index;
};

void render_gui_begin_frame(const RenderGUIBeginFrameInfo& info);
void render_gui_remake_pipelines();
void render_gui_render(const RenderGUIRenderInfo& info);
RenderGUIStats get_render_gui_stats();
void terminate_render_gui();

}