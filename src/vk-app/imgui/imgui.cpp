#include "imgui.hpp"
#include "../vk/command_processor.hpp"
#include "grove/common/common.hpp"
#include "grove/common/scope.hpp"
#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>

#if GROVE_INCLUDE_IMPLOT
#include <implot.h>
#endif

GROVE_NAMESPACE_BEGIN

void vk::destroy_and_terminate_imgui_impl(ImGuiImpl* impl, VkDevice device) {
  if (impl->initialized_for_vulkan) {
    ImGui_ImplVulkan_Shutdown();
    impl->initialized_for_vulkan = false;
  }
  if (impl->initialized_glfw_impl) {
    ImGui_ImplGlfw_Shutdown();
    impl->initialized_glfw_impl = false;
  }
  if (impl->imgui_context) {
#if GROVE_INCLUDE_IMPLOT
    ImPlot::DestroyContext();
#endif
    ImGui::DestroyContext(impl->imgui_context);
    impl->imgui_context = nullptr;
  }
  vk::destroy_descriptor_pool(&impl->descriptor_pool, device);
}

vk::Result<vk::ImGuiImpl> vk::create_and_initialize_imgui_impl(const ImGuiImplCreateInfo& info) {
  vk::ImGuiImpl result;
  bool success = false;
  GROVE_SCOPE_EXIT {
    if (!success) {
      destroy_and_terminate_imgui_impl(&result, info.core.device.handle);
    }
  };

  const uint32_t num_descriptors = 1000;
  const uint32_t max_sets = 1000;
  VkDescriptorPoolSize pool_sizes[] = {
    { VK_DESCRIPTOR_TYPE_SAMPLER, num_descriptors },
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, num_descriptors },
    { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, num_descriptors },
    { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, num_descriptors },
    { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, num_descriptors },
    { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, num_descriptors },
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, num_descriptors },
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, num_descriptors },
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, num_descriptors },
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, num_descriptors },
    { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, num_descriptors }
  };

  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = max_sets;
  pool_info.poolSizeCount = uint32_t(std::size(pool_sizes));
  pool_info.pPoolSizes = pool_sizes;

  if (auto res = vk::create_descriptor_pool(info.core.device.handle, &pool_info)) {
    result.descriptor_pool = res.value;
  } else {
    return error_cast<ImGuiImpl>(res);
  }

  result.imgui_context = ImGui::CreateContext();
#if GROVE_INCLUDE_IMPLOT
  ImPlot::CreateContext();
#endif
  if (ImGui_ImplGlfw_InitForVulkan(info.window, true)) {
    result.initialized_glfw_impl = true;
  } else {
    return {VK_ERROR_UNKNOWN, "Failed to initialize IMGUI implementation for GLFW."};
  }

  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = info.core.instance.handle;
  init_info.PhysicalDevice = info.core.physical_device.handle;
  init_info.Device = info.core.device.handle;
  init_info.Queue = info.graphics_queue.handle;
  init_info.DescriptorPool = result.descriptor_pool.handle;
  init_info.MinImageCount = info.image_count;
  init_info.ImageCount = info.image_count;
  init_info.MSAASamples = info.raster_samples;

  if (ImGui_ImplVulkan_Init(&init_info, info.render_pass)) {
    result.initialized_for_vulkan = true;
  } else {
    return {VK_ERROR_UNKNOWN, "Failed to initialize IMGUI implementation for Vulkan."};
  }

  auto submit = [](VkCommandBuffer cmd) { ImGui_ImplVulkan_CreateFontsTexture(cmd); };
  if (auto err = info.uploader->sync_graphics_queue(info.core, std::move(submit))) {
    return error_cast<ImGuiImpl>(err);
  }

  ImGui_ImplVulkan_DestroyFontUploadObjects();
  success = true;
  return result;
}

void vk::imgui_new_frame() {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void vk::imgui_render_frame(VkCommandBuffer cmd) {
  ImGui::Render();
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

bool vk::imgui_want_capture_mouse(const ImGuiImpl* impl) {
  if (impl->imgui_context) {
    auto& io = ImGui::GetIO();
    return io.WantCaptureMouse;
  } else {
    return false;
  }
}

GROVE_NAMESPACE_END
