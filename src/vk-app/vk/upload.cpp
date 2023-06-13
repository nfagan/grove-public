#include "upload.hpp"
#include "command_processor.hpp"
#include "staging_buffer_system.hpp"
#include "grove/common/scope.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Temporary.hpp"

GROVE_NAMESPACE_BEGIN

using namespace vk;

Result<CommandProcessor::CommandFuture>
vk::upload_from_staging_buffer_async(const void** src_data, const ManagedBuffer** dst_buffers,
                                     const BufferCopyDstInfo* dst_infos, uint32_t num_dst_buffers,
                                     const UploadFromStagingBufferContext& context) {
  struct AsyncBufferCopyInfo {
    size_t size;
    size_t dst_offset;
    VkBuffer src;
    VkBuffer dst;
  };

  Allocator* allocator = context.allocator;
  CommandProcessor* command_processor = context.command_processor;
  StagingBufferSystem* staging_buffer_system = context.staging_buffer_system;
  const Core* core = context.core;

  Temporary<ManagedBuffer, 32> managed_buffs;
  ManagedBuffer* stage_buffs = managed_buffs.require(int(num_dst_buffers));
  uint32_t num_stage_buffs_acquired{};

  Temporary<AsyncBufferCopyInfo, 32> buffer_infos;
  AsyncBufferCopyInfo* buffer_info = buffer_infos.require(int(num_dst_buffers));

  bool success = false;
  GROVE_SCOPE_EXIT {
    if (!success) {
      for (uint32_t i = 0; i < num_stage_buffs_acquired; i++) {
        staging_buffer_system->release_sync(std::move(stage_buffs[i]));
      }
    }
  };

  for (uint32_t i = 0; i < num_dst_buffers; i++) {
    auto dst_contents = dst_buffers[i]->contents();

    AsyncBufferCopyInfo* info = buffer_info + i;
    *info = {};
    info->size = dst_contents.size;
    info->dst = dst_contents.buffer.handle;
    if (dst_infos) {
      info->size = dst_infos[i].size;
      info->dst_offset = dst_infos[i].offset;
    }

    if (auto stage_buff = staging_buffer_system->acquire(allocator, info->size)) {
      info->src = stage_buff.value.contents().buffer.handle;
      stage_buff.value.write(src_data[i], info->size);
      stage_buffs[num_stage_buffs_acquired++] = std::move(stage_buff.value);
    } else {
      return {VK_ERROR_UNKNOWN, "Failed to acquire buffers."};
    }
  }

  auto transfer = [buffer_info, num_dst_buffers](VkCommandBuffer cmd) {
    for (uint32_t i = 0; i < num_dst_buffers; i++) {
      auto& info = buffer_info[i];
      VkBufferCopy copy{};
      copy.size = info.size;
      copy.dstOffset = info.dst_offset;
      VkBuffer src = info.src;
      VkBuffer dst = info.dst;
      vkCmdCopyBuffer(cmd, src, dst, 1, &copy);
    }
  };

  //  Despite being asynchronous, `transfer` will be evaluated immediately with a command buffer
  //  and then discarded, so we can safely capture the temporary `buffer_info`.
  auto res = command_processor->async_graphics_queue(*core, std::move(transfer));
  if (!res) {
    return res;
  }

  success = true;
  for (uint32_t i = 0; i < num_stage_buffs_acquired; i++) {
    staging_buffer_system->release_async(res.value, std::move(stage_buffs[i]));
  }
  return res;
}

bool vk::upload_from_staging_buffer_sync(const void** src_data,
                                         const ManagedBuffer** dst_buffers,
                                         const BufferCopyDstInfo* dst_infos,
                                         uint32_t num_dst_buffers,
                                         const UploadFromStagingBufferContext& context) {
  Allocator* allocator = context.allocator;
  CommandProcessor* command_processor = context.command_processor;
  StagingBufferSystem* staging_buffer_system = context.staging_buffer_system;
  const Core* core = context.core;

  Temporary<ManagedBuffer, 32> managed_buffs;
  ManagedBuffer* stage_buffs = managed_buffs.require(int(num_dst_buffers));

  const auto get_dst_size_off = [&dst_buffers, &dst_infos](uint32_t i, size_t* size, size_t* off) {
    size_t dst_size = dst_buffers[i]->contents().size;
    size_t dst_off{};
    if (dst_infos) {
      auto& dst_info = dst_infos[i];
      dst_size = dst_info.size;
      dst_off = dst_info.offset;
    }
    *size = dst_size;
    *off = dst_off;
  };

  uint32_t num_stage_buffs_acquired{};
  bool success{true};
  for (uint32_t i = 0; i < num_dst_buffers; i++) {
    size_t size{};
    size_t dst_off{};
    get_dst_size_off(i, &size, &dst_off);
    if (auto stage_buff = staging_buffer_system->acquire(allocator, size)) {
      stage_buff.value.write(src_data[i], size);
      stage_buffs[num_stage_buffs_acquired++] = std::move(stage_buff.value);
    } else {
      success = false;
      break;
    }
  }

  if (success) {
    auto transfer = [&](VkCommandBuffer cmd) {
      for (uint32_t i = 0; i < num_dst_buffers; i++) {
        const auto dst_contents = dst_buffers[i]->contents();
        size_t size{};
        size_t dst_off{};
        get_dst_size_off(i, &size, &dst_off);
        VkBufferCopy copy{};
        copy.size = size;
        copy.dstOffset = dst_off;
        VkBuffer src = stage_buffs[i].contents().buffer.handle;
        VkBuffer dst = dst_contents.buffer.handle;
        vkCmdCopyBuffer(cmd, src, dst, 1, &copy);
      }
    };

    if (auto err = command_processor->sync_graphics_queue(*core, std::move(transfer))) {
      GROVE_ASSERT(false);
      success = false;
      (void) err;
    }
  }

  for (uint32_t i = 0; i < num_stage_buffs_acquired; i++) {
    staging_buffer_system->release_sync(std::move(stage_buffs[i]));
  }

  return success;
}

GROVE_NAMESPACE_END
