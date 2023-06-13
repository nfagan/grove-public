#pragma once

#include "../vk/vk.hpp"
#include "../vk/simple_descriptor_system.hpp"
#include "../vk/profiler.hpp"
#include "SampledImageManager.hpp"
#include "DynamicSampledImageManager.hpp"
#include "shadow_pass.hpp"
#include "post_process_pass.hpp"
#include "forward_write_back_pass.hpp"
#include "post_forward_pass.hpp"
#include "present_pass.hpp"

namespace grove::vk {

struct GraphicsContext {
  struct SwapchainCommandPools {
    std::vector<CommandPool> pools;
  };

  struct SwapchainSync {
    std::vector<Semaphore> image_available_semaphores;
    std::vector<Semaphore> render_finished_semaphores;
    std::vector<Fence> in_flight_fences;
    std::vector<uint64_t> in_flight_frame_ids;
  };

  struct PresentPassModifications {
    Optional<bool> set_enabled;
    Optional<VkExtent2D> set_internal_forward_resolution;
  };

  const uint32_t frame_queue_depth{2};
  int desired_msaa_samples{};
  bool need_recreate_swapchain{};

  bool present_pass_enabled{};
  VkExtent2D internal_forward_resolution{1280, 720};
  PresentPassModifications present_pass_modifications;

  RenderFrameInfo frame_info{};
  Core core;
  Allocator allocator;
  Swapchain swapchain;

  ForwardWriteBackPass forward_write_back_pass;
  PostForwardPass post_forward_pass;
  PostProcessPass post_process_pass;
  ShadowPass shadow_pass;
  PresentPass present_pass;

  PipelineSystem pipeline_system;
  BufferSystem buffer_system;
  CommandProcessor command_processor;
  DescriptorSystem descriptor_system;
  SimpleDescriptorSystem simple_descriptor_system;
  StagingBufferSystem staging_buffer_system;
  SamplerSystem sampler_system;
  Profiler graphics_profiler;

  SampledImageManager sampled_image_manager;
  DynamicSampledImageManager dynamic_sampled_image_manager;

  SwapchainSync swapchain_sync;
  SwapchainCommandPools swapchain_command_pools;

  VkClearValue store_clear_values[8];
};

struct GraphicsContextCreateInfo {
  InstanceCreateInfo instance_create_info{};
  GLFWwindow* window{};
  int desired_num_msaa_samples{4};
};

struct TopOfRenderResult {
  VkFence in_flight_fence;
  VkSemaphore image_available_semaphore;
  VkSemaphore render_finished_semaphore;
  uint32_t frame_index;
};

struct AcquireNextImageResult {
  uint32_t image_index;
  bool need_recreate_swapchain;
};

struct PresentResult {
  bool need_recreate_swapchain;
};

struct BeginPassResult {
  VkRenderPassBeginInfo pass_begin_info;
  VkViewport viewport;
  VkRect2D scissor;
};

InstanceCreateInfo make_default_instance_create_info();

Error create_graphics_context(GraphicsContext* context, const GraphicsContextCreateInfo* create_info);
void destroy_graphics_context(GraphicsContext* context);

void set_present_pass_enabled(GraphicsContext* context, bool value);
bool get_present_pass_enabled(const GraphicsContext* context);

void set_internal_forward_resolution(GraphicsContext* context, VkExtent2D extent);
VkExtent2D get_internal_forward_resolution(const GraphicsContext* context);
VkExtent2D get_forward_pass_render_image_resolution(const GraphicsContext* context);

PipelineRenderPassInfo make_forward_pass_pipeline_render_pass_info(const GraphicsContext* context);
PipelineRenderPassInfo make_post_forward_pass_pipeline_render_pass_info(const GraphicsContext* context);
PipelineRenderPassInfo make_shadow_pass_pipeline_render_pass_info(const GraphicsContext* context);
PipelineRenderPassInfo make_post_process_pipeline_render_pass_info(const GraphicsContext* context);

Result<TopOfRenderResult> top_of_render(GraphicsContext* context, GLFWwindow* window);
Result<AcquireNextImageResult> acquire_next_image(GraphicsContext* context,
                                                  VkSemaphore image_avail_sema);

BeginPassResult begin_forward_pass(GraphicsContext* context);
BeginPassResult begin_post_forward_pass(GraphicsContext* context);
BeginPassResult begin_post_process_pass(GraphicsContext* context, uint32_t image_index);
BeginPassResult begin_present_pass(GraphicsContext* context, uint32_t image_index);

Error end_render_graphics_queue_submit(GraphicsContext* context,
                                       VkCommandBuffer cmd,
                                       VkFence in_flight_fence,
                                       VkSemaphore image_avail_sema,
                                       VkSemaphore render_finished_sema);
Result<PresentResult> present_frame(GraphicsContext* context,
                                    uint32_t image_index,
                                    VkSemaphore render_finished_sema);
Error end_frame(GraphicsContext* context,
                uint32_t image_index,
                VkCommandBuffer cmd,
                VkFence in_flight_fence,
                VkSemaphore image_avail_sema,
                VkSemaphore render_finished_sema);

}