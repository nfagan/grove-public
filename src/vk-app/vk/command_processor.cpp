#include "command_processor.hpp"
#include "submit.hpp"
#include "grove/common/common.hpp"
#include "grove/vk/core.hpp"

GROVE_NAMESPACE_BEGIN

using namespace vk;

void vk::CommandProcessor::begin_frame(VkDevice device) {
  contexts_examined_this_frame.clear();
  auto pend_it = pending_futures.begin();
  while (pend_it != pending_futures.end()) {
    auto& pend = *pend_it;
    const auto& indices = pend.indices;
    bool future_is_ready{};
    if (auto it = contexts_examined_this_frame.find(indices);
        it != contexts_examined_this_frame.end()) {
      future_is_ready = it->second;
    } else {
      //  Only check the status of a fence once per frame, per unique context / fence.
      auto& pool_ctx = pool_contexts[indices.pool];
      auto& ctx = pool_ctx.contexts[indices.command];
      VkFence fence = ctx.fence.handle;
      auto res = vkWaitForFences(device, 1, &fence, VK_TRUE, 0);
      if (res == VK_SUCCESS) {
        vkResetFences(device, 1, &ctx.fence.handle);
        on_context_complete(device, indices.pool, indices.command);
        future_is_ready = true;
      }
      contexts_examined_this_frame[indices] = future_is_ready;
    }
    if (future_is_ready) {
      pend.future->mark_ready();
      pend_it = pending_futures.erase(pend_it);
    } else {
      ++pend_it;
    }
  }
}

void vk::CommandProcessor::end_frame(VkDevice device) {
  for (auto& pend : pending_submit) {
    auto& pool_context = pool_contexts[pend.pool];
    auto& command_context = pool_context.contexts[pend.command];
    VkCommandBuffer cmd = command_context.cmd;
    VkQueue queue = command_context.queue;
    VkFence fence = command_context.fence.handle;
    GROVE_VK_CHECK_ERR(end_command_buffer(cmd))
    GROVE_VK_CHECK_ERR(vk::queue_submit(cmd, queue, fence))
    on_context_submit(device, pend.pool, pend.command);
  }
  pending_submit.clear();
}

void vk::CommandProcessor::destroy(VkDevice device) {
  for (auto& ctx : pool_contexts) {
    vk::destroy_command_pool(&ctx.command_pool, device);
    for (auto& sub_ctx : ctx.contexts) {
      vk::destroy_fence(&sub_ctx.fence, device);
    }
  }
}

void vk::CommandProcessor::on_context_begin(VkDevice,
                                            VkQueue queue,
                                            uint32_t pool_ind,
                                            uint32_t command_ind) {
  auto& pool_context = pool_contexts[pool_ind];
  auto& ctx = pool_context.contexts[command_ind];
  GROVE_ASSERT(!ctx.began && ctx.queue == VK_NULL_HANDLE &&
               !ctx.submitted && pool_context.num_submitted < command_pool_size);
  const auto info = make_command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  GROVE_VK_CHECK_ERR(begin_command_buffer(ctx.cmd, &info))
  ctx.began = true;
  ctx.queue = queue;
}

void vk::CommandProcessor::on_context_submit(VkDevice,
                                             uint32_t pool_ind,
                                             uint32_t command_ind) {
  auto& pool_context = pool_contexts[pool_ind];
  auto& ctx = pool_context.contexts[command_ind];
  GROVE_ASSERT(ctx.queue && !ctx.submitted && pool_context.num_submitted < command_pool_size);
  ctx.submitted = true;
  pool_context.num_submitted++;
}

void vk::CommandProcessor::on_context_complete(VkDevice device,
                                               uint32_t pool_ind,
                                               uint32_t command_ind) {
  auto& pool_context = pool_contexts[pool_ind];
  auto& completed_ctx = pool_context.contexts[command_ind];
  GROVE_ASSERT(completed_ctx.submitted &&
               pool_context.num_submitted > 0 &&
               pool_context.num_complete < command_pool_size);
  completed_ctx.complete = true;
  pool_context.num_complete++;
  if (pool_context.num_submitted == pool_context.num_complete) {
    //  Reset the pool.
    for (auto& ctx : pool_context.contexts) {
      if (ctx.submitted) {
        GROVE_ASSERT(ctx.complete);
      }
      ctx.submitted = false;
      ctx.complete = false;
      ctx.began = false;
      ctx.queue = VK_NULL_HANDLE;
    }
    pool_context.num_submitted = 0;
    pool_context.num_complete = 0;
    vk::reset_command_pool(device, pool_context.command_pool.handle);
  }
}

bool vk::CommandProcessor::require_context(VkDevice device,
                                           uint32_t queue_family,
                                           VkQueue queue,
                                           ContextIndices* indices) {
  //  @TODO: Use free list
  for (auto& context : pool_contexts) {
    if (context.command_pool.queue_family == queue_family &&
        context.num_submitted < command_pool_size) {
      for (auto& ctx : context.contexts) {
        if (ctx.queue == queue && !ctx.submitted) {
          indices->pool = uint32_t(&context - pool_contexts.data());
          indices->command = uint32_t(&ctx - context.contexts.data());
          return true;
        }
      }
    }
  }

  auto cmd_res = create_command_pool(device, queue_family, command_pool_size);
  if (!cmd_res) {
    return false;
  }

  auto fence_res = vk::create_fences(device, command_pool_size, 0);
  if (!fence_res) {
    vk::destroy_command_pool(&cmd_res.value, device);
    return false;
  }

  auto& new_context = pool_contexts.emplace_back();
  uint32_t i{};
  for (auto& sub_ctx : new_context.contexts) {
    sub_ctx.cmd = cmd_res.value.ith_command_buffer(i)->handle;
    sub_ctx.fence = fence_res.value[i];
    i++;
  }
  new_context.command_pool = std::move(cmd_res.value);
  indices->pool = uint32_t(pool_contexts.size() - 1);
  indices->command = 0;
  return true;
}

Error vk::CommandProcessor::sync_graphics_queue(const Core& core,
                                                Command&& command,
                                                uint32_t ith_queue) {
  const DeviceQueue* queue{};
  uint32_t queue_family{};
  if (core.ith_graphics_queue_and_family(&queue, &queue_family, ith_queue)) {
    return sync(core.device.handle, queue->handle, queue_family, std::move(command));
  }
  return {VK_ERROR_UNKNOWN, "Failed to find acceptable queue."};
}

Result<CommandProcessor::CommandFuture>
vk::CommandProcessor::async_graphics_queue(const Core& core,
                                           Command&& command,
                                           uint32_t ith_queue) {
  const DeviceQueue* queue{};
  uint32_t queue_family{};
  if (core.ith_graphics_queue_and_family(&queue, &queue_family, ith_queue)) {
    return async(core.device.handle, queue->handle, queue_family, std::move(command));
  }
  return {VK_ERROR_UNKNOWN, "Failed to find acceptable queue."};
}

Error vk::CommandProcessor::sync(VkDevice device,
                                 VkQueue queue,
                                 uint32_t queue_family,
                                 Command&& command) {
  ContextIndices ctx_inds{};
  if (!require_context(device, queue_family, VK_NULL_HANDLE, &ctx_inds)) {
    GROVE_ASSERT(false);
    return {VK_ERROR_UNKNOWN, "Failed to acquire command context."};
  }

  auto& pool_context = pool_contexts[ctx_inds.pool];
  VkCommandBuffer cmd = pool_context.contexts[ctx_inds.command].cmd;
  VkFence fence = pool_context.contexts[ctx_inds.command].fence.handle;

  on_context_begin(device, queue, ctx_inds.pool, ctx_inds.command);
  command(cmd);
  GROVE_VK_CHECK_ERR(end_command_buffer(cmd))
  GROVE_VK_CHECK_ERR(submit_sync(device, cmd, queue, fence))
  on_context_submit(device, ctx_inds.pool, ctx_inds.command);
  on_context_complete(device, ctx_inds.pool, ctx_inds.command);
  return {};
}

Result<CommandProcessor::CommandFuture>
vk::CommandProcessor::async(VkDevice device,
                            VkQueue queue,
                            uint32_t queue_family,
                            Command&& command) {
  ContextIndices ctx_indices{};
  if (!require_context(device, queue_family, queue, &ctx_indices)) {
    GROVE_ASSERT(false);
    return {VK_ERROR_UNKNOWN, "Failed to acquire command context."};
  }

  auto& pool_context = pool_contexts[ctx_indices.pool];
  auto& command_context = pool_context.contexts[ctx_indices.command];
  VkCommandBuffer cmd = command_context.cmd;
  if (!command_context.began) {
    on_context_begin(device, queue, ctx_indices.pool, ctx_indices.command);
    pending_submit.insert(ctx_indices);
  }

  command(cmd);
  auto fut = std::make_shared<FutureError>();
  PendingFuture pending{};
  pending.future = fut;
  pending.indices = ctx_indices;
  pending_futures.push_back(std::move(pending));
  return fut;
}

GROVE_NAMESPACE_END
