#pragma once

#include "grove/vk/common.hpp"
#include "grove/vk/buffer.hpp"
#include "command_processor.hpp"

namespace grove::vk {

struct Core;
class CommandProcessor;
class StagingBufferSystem;

struct BufferCopyDstInfo {
  size_t size;
  size_t offset;
};

struct UploadFromStagingBufferContext {
  const Core* core;
  Allocator* allocator;
  StagingBufferSystem* staging_buffer_system;
  CommandProcessor* command_processor;
};

[[nodiscard]] bool upload_from_staging_buffer_sync(const void** src_data,
                                                   const ManagedBuffer** dst_buffers,
                                                   const BufferCopyDstInfo* dst_infos,
                                                   uint32_t num_dst_buffers,
                                                   const UploadFromStagingBufferContext& context);

[[nodiscard]] Result<CommandProcessor::CommandFuture>
upload_from_staging_buffer_async(const void** src_data,
                                 const ManagedBuffer** dst_buffers,
                                 const BufferCopyDstInfo* dst_infos,
                                 uint32_t num_dst_buffers,
                                 const UploadFromStagingBufferContext& context);

inline UploadFromStagingBufferContext
make_upload_from_staging_buffer_context(const Core* core,
                                        Allocator* allocator,
                                        StagingBufferSystem* staging_buffer_system,
                                        CommandProcessor* command_processor) {
  return {core, allocator, staging_buffer_system, command_processor};
}

}