#include "AudioBufferStore.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using BackingStoreType = audio::BufferBackingStoreType;

AudioBufferChunk make_single_chunk(const AudioBufferDescriptor& descriptor, unsigned char* data) {
  AudioBufferChunk result{};
  result.descriptor = descriptor;
  result.data = data;
  result.frame_offset = 0;
  result.frame_size = descriptor.size / descriptor.layout.stride();
  return result;
}

} //  anon

bool AudioBufferStore::ui_list(std::vector<BufferInfo>& into) const {
  if (in_memory_audio_buffer_accessor.writer_can_modify()) {
    //  I.e., we're not waiting for the audio thread to swap read and write pointers.
    for (auto& [handle, chunk] : *in_memory_audio_buffer_accessor.writer_ptr()) {
      BufferInfo info{};
      info.descriptor = chunk.descriptor;
      info.handle = handle;
      into.push_back(info);
    }

    return true;
  } else {
    return false;
  }
}

void AudioBufferStore::ui_update() {
  if (!pending_ui_submit.empty()) {
    if (auto* write_to = in_memory_audio_buffer_accessor.writer_begin_modification()) {
      for (auto& pend : pending_ui_submit) {
        if (pend.handle.backing_store_type == BackingStoreType::InMemory) {
          //  In memory backing store.
          if (pend.type == CommandType::Add) {
            auto chunk = make_single_chunk(pend.descriptor, pend.data);
            //  Add the chunk to the writer's buffer store.
            write_to->emplace(pend.handle, chunk);

          } else if (pend.type == CommandType::Remove) {
            if (write_to->erase(pend.handle) > 0) {
              pend.ui_remove_future->data.success = true;
            } else {
              //  No such handle.
              pend.ui_remove_future->data.success = false;
            }
          } else {
            //  Other command types not handled.
            assert(false);
          }
          //  Wait for confirmation from the audio thread.
          pending_reader_swap.push_back(pend);
        } else {
          //  Other backing store types not yet implemented.
          assert(false);
        }
      }

      pending_ui_submit.clear();
    }
  }

  auto update_res = in_memory_audio_buffer_accessor.writer_update();
  if (update_res.changed) {
    //  The audio thread has now seen all pending modifications.
    for (auto& pend : pending_reader_swap) {
      if (pend.type == CommandType::Add) {
        pend.ui_add_future->mark_ready();

      } else if (pend.type == CommandType::Remove) {
        //  Free memory.
        in_memory_backing_store.erase(pend.handle);
        pend.ui_remove_future->mark_ready();

      } else {
        assert(false);
      }
    }
    pending_reader_swap.clear();
  }
}

void AudioBufferStore::render_update() {
  in_memory_audio_buffer_accessor.reader_maybe_swap();
}

std::unique_ptr<Future<AudioBufferHandle>>
AudioBufferStore::ui_add_in_memory(const AudioBufferDescriptor& descriptor,
                                   std::unique_ptr<unsigned char[]> backing_store_data) {
  auto* to_store_ptr = backing_store_data.get();
  AudioBufferHandle handle{next_buffer_handle_id++, audio::BufferBackingStoreType::InMemory};
  in_memory_backing_store[handle] = std::move(backing_store_data);

  auto fut = std::make_unique<Future<AudioBufferHandle>>();
  fut->data = handle;

  Command command{};
  command.type = CommandType::Add;
  command.data = to_store_ptr;
  command.handle = handle;
  command.descriptor = descriptor;
  command.ui_add_future = fut.get();

  pending_ui_submit.push_back(command);
  return fut;
}

std::unique_ptr<Future<AudioBufferHandle>>
AudioBufferStore::ui_add_in_memory(const AudioBufferDescriptor& descriptor, const void* data) {
  auto to_store = std::make_unique<unsigned char[]>(descriptor.size);
  if (descriptor.size > 0) {
    std::memcpy(to_store.get(), data, descriptor.size);
  }
  return ui_add_in_memory(descriptor, std::move(to_store));
}

std::unique_ptr<Future<AudioBufferStore::RemoveResult>>
AudioBufferStore::ui_remove(AudioBufferHandle handle) {
  auto fut = std::make_unique<Future<RemoveResult>>();
  fut->data.handle = handle;

  Command command{};
  command.type = CommandType::Remove;
  command.handle = handle;
  command.ui_remove_future = fut.get();

  pending_ui_submit.push_back(command);
  return fut;
}

Optional<AudioBufferChunk>
AudioBufferStore::render_get(AudioBufferHandle handle, uint64_t frame_begin, uint64_t frame_end) const {
  if (handle.backing_store_type == BackingStoreType::InMemory) {
    const auto& store = in_memory_audio_buffer_accessor.read();
    auto it = store.find(handle);
    if (it == store.end()) {
      return NullOpt{};
    }

    (void) frame_begin;
    (void) frame_end;
    return Optional<AudioBufferChunk>(it->second);

  } else {
    //  Not yet supported.
    return NullOpt{};
  }
}

Optional<AudioBufferChunk> AudioBufferStore::ui_load(AudioBufferHandle handle) const {
  if (handle.backing_store_type == BackingStoreType::InMemory) {
    if (in_memory_audio_buffer_accessor.writer_can_modify()) {
      const auto& writer_store = *in_memory_audio_buffer_accessor.writer_ptr();
      auto it = writer_store.find(handle);
      if (it == writer_store.end()) {
        return NullOpt{};
      } else {
        assert(it->second.is_complete());
        return Optional<AudioBufferChunk>(it->second);
      }
    } else {
      //  We're waiting for the audio thread to non-atomically swap the read and write pointers,
      //  so it's not safe to access any part of the buffer.
      return NullOpt{};
    }

  } else {
    //  Other types not yet handled.
    return NullOpt{};
  }
}

GROVE_NAMESPACE_END
