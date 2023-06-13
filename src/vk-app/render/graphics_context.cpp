#include "graphics_context.hpp"
#include "debug_label.hpp"
#include "grove/common/common.hpp"
#include "grove/common/scope.hpp"
#include "shadow.hpp"
#include "grove/common/platform.hpp"

GROVE_NAMESPACE_BEGIN

using namespace vk;

namespace {

using SwapchainCommandPools = GraphicsContext::SwapchainCommandPools;
using SwapchainSync = GraphicsContext::SwapchainSync;

void destroy_swapchain_sync(SwapchainSync* sync, VkDevice device) {
  destroy_semaphores(&sync->image_available_semaphores, device);
  destroy_semaphores(&sync->render_finished_semaphores, device);
  destroy_fences(&sync->in_flight_fences, device);
  sync->in_flight_frame_ids = {};
}

void destroy_swapchain_command_pools(SwapchainCommandPools* pools, VkDevice device) {
  destroy_command_pools(&pools->pools, device);
}

void destroy_swapchain_components(GraphicsContext* context) {
  auto& device = context->core.device;
  destroy_shadow_pass(&context->shadow_pass, device.handle);
  destroy_forward_write_back_pass(&context->forward_write_back_pass, device.handle);
  destroy_post_forward_pass(&context->post_forward_pass, device.handle);
  destroy_post_process_pass(&context->post_process_pass, device.handle);
  destroy_present_pass(&context->present_pass, device.handle);
  destroy_swapchain(&context->swapchain, device.handle);
}

Result<Core> create_vulkan_core(GLFWwindow* window, const InstanceCreateInfo& info) {
  const std::array<const char*, 5> device_extensions{{
    VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME,
    "VK_KHR_depth_stencil_resolve",
    "VK_KHR_maintenance2",
    "VK_KHR_create_renderpass2",
    "VK_KHR_multiview",
//    "VK_KHR_push_descriptor"
  }};
  vk::CoreCreateInfo core_create_info{};
  core_create_info.window = window;
  core_create_info.instance_create_info = info;
  for (const char* ext : device_extensions) {
    core_create_info.additional_required_physical_device_extensions.push_back(ext);
  }
  return create_core(core_create_info);
}

void initialize_allocator(Allocator* alloc, const vk::Core& core) {
  alloc->create(&core.instance, &core.physical_device, &core.device);
}

Error initialize_graphics_profiler(vk::Profiler* profiler,
                                   const vk::Core& core,
                                   uint32_t frame_queue_depth) {
  if (core.physical_device.queue_family_indices.graphics) {
    uint32_t queue_fam = core.physical_device.queue_family_indices.graphics.value();
    profiler->initialize(core.device.handle, core.physical_device, queue_fam, frame_queue_depth);
    vk::Profiler::set_global_profiler(profiler);
    return {};
  } else {
    return {VK_ERROR_INITIALIZATION_FAILED, "Failed to initialize graphics profiler."};
  }
}

Error initialize_swapchain(vk::Swapchain* swapchain, const vk::Core& core, GLFWwindow* window) {
  auto fb_dims = vk::get_framebuffer_dimensions(window);
  auto res = vk::create_swapchain(core.physical_device, core.device, core.surface, fb_dims);
  if (res) {
    *swapchain = std::move(res.value);
    return {};
  } else {
    return {res.status, res.message};
  }
}

Error initialize_forward_write_back_pass(vk::ForwardWriteBackPass* pass, GraphicsContext& context) {
  const auto image_samples = vk::choose_forward_write_back_pass_samples(
    context.core.physical_device,
    context.desired_msaa_samples);
  VkFormat depth_format;
  if (auto format = vk::choose_forward_write_back_pass_depth_format(
    context.core.physical_device.handle)) {
    depth_format = format.value();
  } else {
    return {VK_ERROR_INITIALIZATION_FAILED, "No suitable depth format."};
  }
  VkResolveModeFlagBits depth_resolve_mode;
  if (auto mode = vk::choose_forward_write_back_pass_depth_resolve_mode(context.core.physical_device)) {
    depth_resolve_mode = mode.value();
  } else {
    return {VK_ERROR_INITIALIZATION_FAILED, "No suitable depth resolve mode."};
  }

  const auto extent = context.present_pass_enabled ?
    context.internal_forward_resolution : context.swapchain.extent;

  vk::ForwardWriteBackPassCreateInfo create_info{};
  create_info.instance = context.core.instance.handle;
  create_info.device = context.core.device.handle;
  create_info.allocator = &context.allocator;
  create_info.color_format = context.swapchain.image_format;
  create_info.depth_format = depth_format;
  create_info.image_extent = extent;
  create_info.image_samples = image_samples;
  create_info.depth_resolve_mode = depth_resolve_mode;
  auto res = vk::create_forward_write_back_pass(&create_info);
  if (!res) {
    return {res.status, res.message};
  } else {
    *pass = std::move(res.value);
    return {};
  }
}

Error initialize_post_forward_pass(vk::PostForwardPass* pass, GraphicsContext& context) {
  //  @NOTE: Must initialize after forward pass
  auto& depth_im_view = context.forward_write_back_pass.single_sample_depth_image_view;
  auto& color_im_view = context.forward_write_back_pass.single_sample_color_image_view;
  assert(depth_im_view.is_valid() && color_im_view.is_valid());

  vk::PostForwardPassCreateInfo create_info{};
  create_info.device = context.core.device.handle;
  create_info.single_sample_color_image_view = color_im_view.contents().handle;
  create_info.single_sample_depth_image_view = depth_im_view.contents().handle;
  create_info.image_extent = context.forward_write_back_pass.image_extent;
  create_info.color_format = context.forward_write_back_pass.color_image_format;
  create_info.depth_format = context.forward_write_back_pass.depth_image_format;
  auto res = vk::create_post_forward_pass(&create_info);
  if (!res) {
    return {res.status, res.message};
  } else {
    *pass = std::move(res.value);
    return {};
  }
}

Error initialize_post_process_pass(vk::PostProcessPass* pass, GraphicsContext& context) {
  VkFormat depth_format;
  if (auto format = vk::choose_post_process_pass_depth_format(
    context.core.physical_device.handle)) {
    depth_format = format.value();
  } else {
    return {VK_ERROR_INITIALIZATION_FAILED, "No suitable depth format."};
  }

  vk::PostProcessPassCreateInfo create_info{};
  create_info.device = context.core.device.handle;
  create_info.allocator = &context.allocator;

  std::vector<VkImageView> image_views;
  if (!context.present_pass_enabled) {
    for (auto& view : context.swapchain.image_views) {
      image_views.push_back(view.handle);
    }
    create_info.present_image_views = image_views.data();
    create_info.num_present_image_views = uint32_t(image_views.size());
    create_info.image_extent = context.swapchain.extent;
  } else {
    create_info.separate_present_pass_enabled = true;
    create_info.image_extent = context.internal_forward_resolution;
  }

  create_info.color_format = context.swapchain.image_format;
  create_info.depth_format = depth_format;

  auto res = vk::create_post_process_pass(&create_info);
  if (!res) {
    return {res.status, res.message};
  } else {
    *pass = std::move(res.value);
    return {};
  }
}

Error initialize_present_pass(vk::PresentPass* pass, GraphicsContext& context) {
  VkFormat depth_format;
  if (auto format = vk::choose_post_process_pass_depth_format(context.core.physical_device.handle)) {
    depth_format = format.value();
  } else {
    return {VK_ERROR_INITIALIZATION_FAILED, "No suitable depth format."};
  }

  std::vector<VkImageView> image_views;
  for (auto& view : context.swapchain.image_views) {
    image_views.push_back(view.handle);
  }

  vk::PresentPassCreateInfo create_info{};
  create_info.device = context.core.device.handle;
  create_info.allocator = &context.allocator;
  create_info.present_image_views = image_views.data();
  create_info.num_present_image_views = uint32_t(image_views.size());
  create_info.color_format = context.swapchain.image_format;
  create_info.depth_format = depth_format;
  create_info.image_extent = context.swapchain.extent;

  auto res = vk::create_present_pass(&create_info);
  if (!res) {
    return {res.status, res.message};
  } else {
    *pass = std::move(res.value);
    return {};
  }
}

Error initialize_shadow_pass(vk::ShadowPass* pass, GraphicsContext& context) {
  auto depth_format_res = vk::choose_shadow_pass_image_format(context.core.physical_device);
  if (!depth_format_res) {
    return {VK_ERROR_INITIALIZATION_FAILED, "No suitable depth format found."};
  }
  vk::CreateShadowPassInfo info{};
  info.depth_format = depth_format_res.value();
  info.allocator = &context.allocator;
  info.device = context.core.device.handle;
  info.image_dim = 1024;
  info.num_layers = GROVE_NUM_SUN_SHADOW_CASCADES;
  info.samples = VK_SAMPLE_COUNT_1_BIT;
  auto res = vk::create_shadow_pass(&info);
  if (!res) {
    return {res.status, res.message};
  } else {
    *pass = std::move(res.value);
    return {};
  }
}

Result<SwapchainCommandPools> create_swapchain_command_pools(VkDevice device,
                                                             uint32_t graphics_queue,
                                                             uint32_t num_pools) {
  SwapchainCommandPools result;
  for (uint32_t i = 0; i < num_pools; i++) {
    auto pool_res = vk::create_command_pool(device, graphics_queue, 1);
    if (pool_res) {
      result.pools.push_back(std::move(pool_res.value));
    } else {
      destroy_swapchain_command_pools(&result, device);
      return error_cast<SwapchainCommandPools>(pool_res);
    }
  }
  return result;
}

Result<SwapchainSync> create_swapchain_sync(VkDevice device, uint32_t num_frames) {
  SwapchainSync result{};
  bool success = false;
  GROVE_SCOPE_EXIT {
    if (!success) {
      destroy_swapchain_sync(&result, device);
    }
  };
  result.in_flight_frame_ids.resize(num_frames);
  if (auto res = create_semaphores(device, num_frames)) {
    result.image_available_semaphores = std::move(res.value);
  } else {
    return error_cast<SwapchainSync>(res);
  }
  if (auto res = create_semaphores(device, num_frames)) {
    result.render_finished_semaphores = std::move(res.value);
  } else {
    return error_cast<SwapchainSync>(res);
  }
  if (auto res = create_fences(device, num_frames, VK_FENCE_CREATE_SIGNALED_BIT)) {
    result.in_flight_fences = std::move(res.value);
  } else {
    return error_cast<SwapchainSync>(res);
  }
  success = true;
  return result;
}

Error initialize_swapchain_sync(SwapchainSync* sync, const GraphicsContext& context) {
  auto res = create_swapchain_sync(context.core.device.handle, context.frame_queue_depth);
  if (res) {
    *sync = std::move(res.value);
    return {};
  } else {
    return {res.status, res.message};
  }
}

Error initialize_swapchain_command_pools(GraphicsContext::SwapchainCommandPools* pools,
                                         const GraphicsContext& context) {
  const uint32_t num_pools = context.swapchain.num_image_views();
  auto graphics_queue = context.core.physical_device.queue_family_indices.graphics.unwrap();
  auto device = context.core.device.handle;
  if (auto res = create_swapchain_command_pools(device, graphics_queue, num_pools)) {
    *pools = std::move(res.value);
    return {};
  } else {
    return {res.status, res.message};
  }
}

Error create_swapchain_components(GraphicsContext* context, GLFWwindow* window) {
  GROVE_VK_TRY_ERR(initialize_swapchain(&context->swapchain, context->core, window))
  GROVE_VK_TRY_ERR(initialize_shadow_pass(&context->shadow_pass, *context))
  GROVE_VK_TRY_ERR(initialize_forward_write_back_pass(&context->forward_write_back_pass, *context))
  GROVE_VK_TRY_ERR(initialize_post_forward_pass(&context->post_forward_pass, *context))
  GROVE_VK_TRY_ERR(initialize_post_process_pass(&context->post_process_pass , *context))
  if (context->present_pass_enabled) {
    GROVE_VK_TRY_ERR(initialize_present_pass(&context->present_pass, *context))
  }
  return {};
}

void tick_frame_info(GraphicsContext* context) {
  auto& frame_info = context->frame_info;
  frame_info.frame_queue_depth = context->frame_queue_depth;
  frame_info.current_frame_index = (frame_info.current_frame_id++) % context->frame_queue_depth;
  auto& finished_id = context->swapchain_sync.in_flight_frame_ids[frame_info.current_frame_index];
  frame_info.finished_frame_id = finished_id;
  finished_id = frame_info.current_frame_id;
}

Error recreate_swapchain_components(vk::GraphicsContext* context, GLFWwindow* window) {
  if (context->core.device.handle) {
    vkDeviceWaitIdle(context->core.device.handle);
    destroy_swapchain_components(context);
    return create_swapchain_components(context, window);
  } else {
    return {VK_ERROR_DEVICE_LOST, "Missing device."};
  }
}

} //  anon

Error vk::create_graphics_context(GraphicsContext* context,
                                  const GraphicsContextCreateInfo* create_info) {
  bool success = false;
  GROVE_SCOPE_EXIT {
    if (!success) {
      destroy_graphics_context(context);
    }
  };

  context->desired_msaa_samples = create_info->desired_num_msaa_samples;
  {
    auto res = create_vulkan_core(create_info->window, create_info->instance_create_info);
    if (!res) {
      return {res.status, res.message};
    } else {
      context->core = std::move(res.value);
    }
  }

  initialize_allocator(&context->allocator, context->core);
  context->descriptor_system.initialize(context->frame_queue_depth);
  context->simple_descriptor_system.initialize(context->core.device.handle, context->frame_queue_depth);
  context->sampled_image_manager.initialize(
    &context->core, &context->allocator, &context->command_processor);
  GROVE_VK_TRY_ERR(initialize_graphics_profiler(
    &context->graphics_profiler, context->core, context->frame_queue_depth))
  GROVE_VK_TRY_ERR(create_swapchain_components(context, create_info->window))
  GROVE_VK_TRY_ERR(initialize_swapchain_sync(&context->swapchain_sync, *context))
  GROVE_VK_TRY_ERR(initialize_swapchain_command_pools(&context->swapchain_command_pools, *context))
  vk::debug::initialize_debug_labels(context->core.instance.handle, context->core.device.handle);
  success = true;
  return {};
}

void vk::destroy_graphics_context(GraphicsContext* context) {
  auto& device = context->core.device;
  context->pipeline_system.terminate(device.handle);
  context->buffer_system.terminate();
  context->descriptor_system.terminate(context->core);
  context->simple_descriptor_system.terminate(context->core.device.handle);
  context->staging_buffer_system.terminate();
  context->sampled_image_manager.destroy();
  context->dynamic_sampled_image_manager.destroy();
  context->sampler_system.terminate(device.handle);
  context->command_processor.destroy(device.handle);
  context->graphics_profiler.terminate();
  destroy_swapchain_command_pools(&context->swapchain_command_pools, device.handle);
  destroy_swapchain_sync(&context->swapchain_sync, device.handle);
  destroy_swapchain_components(context);
  context->allocator.destroy();
  destroy_core(&context->core);
  vk::debug::terminate_debug_labels();
}

Result<TopOfRenderResult> vk::top_of_render(GraphicsContext* context, GLFWwindow* window) {
  tick_frame_info(context);

  if (context->present_pass_modifications.set_enabled) {
    context->present_pass_enabled = context->present_pass_modifications.set_enabled.value();
    context->present_pass_modifications.set_enabled = NullOpt{};
    context->need_recreate_swapchain = true;
  }
  if (context->present_pass_modifications.set_internal_forward_resolution) {
    //  Assume validation handled at site of set()
    context->internal_forward_resolution =
      context->present_pass_modifications.set_internal_forward_resolution.value();
    context->present_pass_modifications.set_internal_forward_resolution = NullOpt{};
    if (context->present_pass_enabled) {
      context->need_recreate_swapchain = true;
    }
  }

  if (context->need_recreate_swapchain) {
    context->need_recreate_swapchain = false;
    if (auto err = recreate_swapchain_components(context, window)) {
      return error_cast<TopOfRenderResult>(err);
    }
  }

  VkDevice device = context->core.device.handle;
  const auto& swap_sync = context->swapchain_sync;
  const uint32_t frame_index = context->frame_info.current_frame_index;
  const auto& in_flight_fence = context->swapchain_sync.in_flight_fences[frame_index];
  if (auto err = vk::wait_fence(device, in_flight_fence.handle, UINT64_MAX)) {
    return error_cast<TopOfRenderResult>(err);
  }
  vk::reset_fences(device, 1, &in_flight_fence.handle);
  //  Begin frame
  context->command_processor.begin_frame(device);
  context->buffer_system.begin_frame(context->frame_info);
  context->descriptor_system.begin_frame(context->core, context->frame_info);
  context->simple_descriptor_system.begin_frame(device, context->frame_info.current_frame_index);
  context->pipeline_system.begin_frame(context->frame_info, device);
  context->sampled_image_manager.begin_frame(context->frame_info);
  context->dynamic_sampled_image_manager.begin_frame(context->frame_info);
  context->staging_buffer_system.begin_frame();

  TopOfRenderResult result{};
  result.in_flight_fence = in_flight_fence.handle;
  result.image_available_semaphore = swap_sync.image_available_semaphores[frame_index].handle;
  result.render_finished_semaphore = swap_sync.render_finished_semaphores[frame_index].handle;
  result.frame_index = frame_index;
  return result;
}

Result<AcquireNextImageResult> vk::acquire_next_image(GraphicsContext* context,
                                                      VkSemaphore image_avail_sema) {
  AcquireNextImageResult result{};
  uint32_t image_index{};
  const auto acq_res = vkAcquireNextImageKHR(
    context->core.device.handle,
    context->swapchain.handle,
    UINT64_MAX,
    image_avail_sema,
    VK_NULL_HANDLE,
    &image_index);
  if (acq_res == VK_SUCCESS || acq_res == VK_SUBOPTIMAL_KHR) {
    result.image_index = image_index;
    return result;
  } else if (acq_res == VK_ERROR_OUT_OF_DATE_KHR) {
    context->need_recreate_swapchain = true;
    result.need_recreate_swapchain = true;
    return result;
  } else {
    return {acq_res, "Failed to acquire swapchain image."};
  }
}

Error vk::end_render_graphics_queue_submit(GraphicsContext* context,
                                           VkCommandBuffer cmd,
                                           VkFence in_flight_fence,
                                           VkSemaphore image_avail_sema,
                                           VkSemaphore render_finished_sema) {
  const auto* graphics_queue = context->core.ith_graphics_queue(0);
  if (!graphics_queue) {
    GROVE_ASSERT(false);
    return {VK_ERROR_UNKNOWN, "Missing graphics queue 0."};
  }

  VkSemaphore wait_for[] = {image_avail_sema};
  VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  VkSemaphore signal[] = {render_finished_sema};
  VkCommandBuffer submit_cmd_buffers[] = {cmd};

  auto submit_info = vk::make_empty_submit_info();
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = wait_for;
  submit_info.pWaitDstStageMask = wait_stages;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = submit_cmd_buffers;
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = signal;

  return vk::queue_submit(graphics_queue->handle, 1, &submit_info, in_flight_fence);
}

Result<PresentResult> vk::present_frame(GraphicsContext* context,
                                        uint32_t image_index,
                                        VkSemaphore render_finished_sema) {
  PresentResult result{};
  auto* present_queue = context->core.ith_present_queue(0);
  if (!present_queue) {
    GROVE_ASSERT(false);
    return {VK_ERROR_UNKNOWN, "Missing present queue 0."};
  }
  VkPresentInfoKHR present_info{};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &render_finished_sema;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &context->swapchain.handle;
  present_info.pImageIndices = &image_index;
  auto present_res = vkQueuePresentKHR(present_queue->handle, &present_info);
  if (present_res == VK_SUCCESS || present_res == VK_SUBOPTIMAL_KHR) {
    return result;
  } else if (present_res == VK_ERROR_OUT_OF_DATE_KHR) {
    context->need_recreate_swapchain = true;
    result.need_recreate_swapchain = true;
    return result;
  } else {
    return {present_res, "Failed to present frame."};
  }
}

Error vk::end_frame(GraphicsContext* context,
                    uint32_t image_index,
                    VkCommandBuffer cmd,
                    VkFence in_flight_fence,
                    VkSemaphore image_avail_sema,
                    VkSemaphore render_finished_sema) {
  context->command_processor.end_frame(context->core.device.handle);
  context->descriptor_system.end_frame(context->core);
  //  graphics submit
  auto submit_err = vk::end_render_graphics_queue_submit(
    context,
    cmd,
    in_flight_fence,
    image_avail_sema,
    render_finished_sema);
  if (submit_err) {
    return submit_err;
  }
  //  present
  auto present_res = vk::present_frame(
    context,
    image_index,
    render_finished_sema);
  if (!present_res) {
    return {present_res.status, present_res.message};
  } else if (present_res.value.need_recreate_swapchain) {
    return {};
  } else {
    return {};
  }
}

BeginPassResult vk::begin_forward_pass(GraphicsContext* context) {
  auto render_begin_info = vk::make_empty_render_pass_begin_info();

  auto& clear_values = context->store_clear_values;
  clear_values[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
  clear_values[1].depthStencil = {0.0f, 0};
  clear_values[2].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
  clear_values[3].depthStencil = {0.0f, 0}; //  write back depth resolve

  render_begin_info.renderPass = context->forward_write_back_pass.render_pass.handle;
  render_begin_info.framebuffer = context->forward_write_back_pass.framebuffer.handle;
  uint32_t num_clear_values = context->forward_write_back_pass.multisampling_enabled() ? 4 : 2;

  auto extent = context->forward_write_back_pass.image_extent;

  render_begin_info.renderArea.extent = extent;
  render_begin_info.clearValueCount = num_clear_values;
  render_begin_info.pClearValues = clear_values;

  const auto viewport = vk::make_full_viewport(extent);
  const auto scissor_rect = vk::make_full_scissor_rect(extent);

  BeginPassResult result{};
  result.scissor = scissor_rect;
  result.viewport = viewport;
  result.pass_begin_info = render_begin_info;
  return result;
}

BeginPassResult vk::begin_post_forward_pass(GraphicsContext* context) {
  auto render_begin_info = vk::make_empty_render_pass_begin_info();

  auto& clear_values = context->store_clear_values;
  clear_values[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
  clear_values[1].depthStencil = {0.0f, 0};

  render_begin_info.renderPass = context->post_forward_pass.render_pass.handle;
  render_begin_info.framebuffer = context->post_forward_pass.framebuffer.handle;
  uint32_t num_clear_values = 0;  //  @NOTE: Loading contents of forward pass, no clearing.

  auto extent = context->forward_write_back_pass.image_extent;

  render_begin_info.renderArea.extent = extent;
  render_begin_info.clearValueCount = num_clear_values;
  render_begin_info.pClearValues = clear_values;

  const auto viewport = vk::make_full_viewport(extent);
  const auto scissor_rect = vk::make_full_scissor_rect(extent);

  BeginPassResult result{};
  result.scissor = scissor_rect;
  result.viewport = viewport;
  result.pass_begin_info = render_begin_info;
  return result;
}

BeginPassResult vk::begin_post_process_pass(GraphicsContext* context, uint32_t image_index) {
  auto render_begin_info = vk::make_empty_render_pass_begin_info();

  auto& clear_values = context->store_clear_values;
  clear_values[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
  clear_values[1].depthStencil = {0.0f, 0};

  auto extent = context->swapchain.extent;
  if (context->present_pass_enabled) {
    assert(context->post_process_pass.framebuffers.size() == 1);
    image_index = 0;
    extent = context->post_process_pass.image_extent;
  }

  render_begin_info.renderPass = context->post_process_pass.render_pass.handle;
  render_begin_info.framebuffer = context->post_process_pass.framebuffers[image_index].handle;
  render_begin_info.renderArea.extent = extent;
  render_begin_info.clearValueCount = 2;
  render_begin_info.pClearValues = clear_values;

  BeginPassResult result{};
  result.viewport = vk::make_full_viewport(extent);
  result.scissor = vk::make_full_scissor_rect(extent);
  result.pass_begin_info = render_begin_info;
  return result;
}

BeginPassResult vk::begin_present_pass(GraphicsContext* context, uint32_t image_index) {
  assert(context->present_pass_enabled);
  assert(image_index < uint32_t(context->present_pass.framebuffers.size()));

  auto render_begin_info = vk::make_empty_render_pass_begin_info();

  auto& clear_values = context->store_clear_values;
  clear_values[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
  clear_values[1].depthStencil = {0.0f, 0};

  auto extent = context->swapchain.extent;

  render_begin_info.renderPass = context->present_pass.render_pass.handle;
  render_begin_info.framebuffer = context->present_pass.framebuffers[image_index].handle;
  render_begin_info.renderArea.extent = extent;
  render_begin_info.clearValueCount = 2;
  render_begin_info.pClearValues = clear_values;

  BeginPassResult result{};
  result.viewport = vk::make_full_viewport(extent);
  result.scissor = vk::make_full_scissor_rect(extent);
  result.pass_begin_info = render_begin_info;
  return result;
}

PipelineRenderPassInfo
vk::make_forward_pass_pipeline_render_pass_info(const GraphicsContext* context) {
  return {
    context->forward_write_back_pass.render_pass.handle,
    0,
    context->forward_write_back_pass.image_samples
  };
}

PipelineRenderPassInfo
vk::make_post_forward_pass_pipeline_render_pass_info(const GraphicsContext* context) {
  return {
    context->post_forward_pass.render_pass.handle,
    0,
    VK_SAMPLE_COUNT_1_BIT
  };
}

PipelineRenderPassInfo
vk::make_shadow_pass_pipeline_render_pass_info(const GraphicsContext* context) {
  return {
    context->shadow_pass.render_pass.handle,
    0,
    context->shadow_pass.raster_samples
  };
}

PipelineRenderPassInfo
vk::make_post_process_pipeline_render_pass_info(const GraphicsContext* context) {
  return PipelineRenderPassInfo{
    context->post_process_pass.render_pass.handle,
    0,
    context->post_process_pass.raster_samples
  };
}

InstanceCreateInfo vk::make_default_instance_create_info() {
  InstanceCreateInfo result{};
#ifdef GROVE_DEBUG
  result.validation_layers_enabled = true;
#ifdef GROVE_WIN
  result.sync_layers_enabled = true;
#endif  //  GROVE_WIN
  result.debug_callback = vk::get_debug_callback();
  result.debug_callback_enabled = true;
  result.debug_report_callback = vk::get_debug_report_callback();
  result.debug_report_callback_enabled = true;
#else
  result.validation_layers_enabled = false;
#endif  //  GROVE_DEBUG
  result.debug_utils_enabled = true;
  result.additional_required_extensions.push_back(
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
  );
  return result;
}

void vk::set_present_pass_enabled(GraphicsContext* context, bool value) {
  context->present_pass_modifications.set_enabled = value;
}

bool vk::get_present_pass_enabled(const GraphicsContext* context) {
  return context->present_pass_enabled;
}

void vk::set_internal_forward_resolution(GraphicsContext* context, VkExtent2D extent) {
  const uint32_t min_res = 128;
  if (extent.width >= min_res && extent.height >= min_res) {
    context->present_pass_modifications.set_internal_forward_resolution = extent;
  }
}

VkExtent2D vk::get_internal_forward_resolution(const GraphicsContext* context) {
  return context->internal_forward_resolution;
}

VkExtent2D vk::get_forward_pass_render_image_resolution(const GraphicsContext* context) {
  return context->present_pass_enabled ?
    context->internal_forward_resolution : context->swapchain.extent;
}

GROVE_NAMESPACE_END
