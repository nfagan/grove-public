#pragma once

#include "grove/vk/vk.hpp"

#define GROVE_INCLUDE_IMPLOT (1)

struct GLFWwindow;
struct ImGuiContext;

namespace grove::vk {

class CommandProcessor;

struct ImGuiImpl {
  vk::DescriptorPool descriptor_pool;
  ImGuiContext* imgui_context{};
  bool initialized_glfw_impl{};
  bool initialized_for_vulkan{};
};

struct ImGuiImplCreateInfo {
  const Core& core;
  const DeviceQueue& graphics_queue;
  CommandProcessor* uploader;
  VkRenderPass render_pass;
  GLFWwindow* window;
  uint32_t image_count;
  VkSampleCountFlagBits raster_samples;
};

Result<ImGuiImpl> create_and_initialize_imgui_impl(const ImGuiImplCreateInfo& info);
void destroy_and_terminate_imgui_impl(ImGuiImpl* impl, VkDevice device);

void imgui_new_frame();
void imgui_dummy_frame();
void imgui_render_frame(VkCommandBuffer cmd);
bool imgui_want_capture_mouse(const ImGuiImpl* impl);

}