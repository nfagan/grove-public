#pragma once

#include "audio_buffer.hpp"
#include "DoubleBuffer.hpp"
#include "grove/common/Future.hpp"
#include "grove/common/Optional.hpp"
#include <memory>
#include <vector>
#include <unordered_map>

namespace grove {

class AudioBufferStore {
public:
  struct BufferInfo {
    AudioBufferHandle handle;
    AudioBufferDescriptor descriptor;
  };

  struct RemoveResult {
    AudioBufferHandle handle;
    bool success{};
  };

private:
  enum class CommandType {
    Add,
    Remove
  };

  struct Command {
    CommandType type{};
    AudioBufferHandle handle;
    AudioBufferDescriptor descriptor;
    unsigned char* data{};

    Future<AudioBufferHandle>* ui_add_future{};
    Future<RemoveResult>* ui_remove_future{};
  };

  using InMemoryAudioBuffers_ =
    std::unordered_map<AudioBufferHandle, AudioBufferChunk, AudioBufferHandle::Hash>;
  using InMemoryAudioBuffers = audio::DoubleBuffer<InMemoryAudioBuffers_>;

  using InMemoryBackingStore =
    std::unordered_map<AudioBufferHandle, std::unique_ptr<unsigned char[]>, AudioBufferHandle::Hash>;

  struct InMemoryAccessorTraits {
    //  Copy the map of in-memory chunks.
    static inline InMemoryAudioBuffers_* on_reader_swap(InMemoryAudioBuffers_* write_to,
                                                        const InMemoryAudioBuffers_* read_from) {
      *write_to = *read_from;
      return write_to;
    }
  };

  using InMemoryAccessor =
    audio::DoubleBufferAccessor<InMemoryAudioBuffers_, InMemoryAccessorTraits>;

public:
  void ui_update();
  bool ui_list(std::vector<BufferInfo>& into) const;

  void render_update();

  std::unique_ptr<Future<AudioBufferHandle>>
  ui_add_in_memory(const AudioBufferDescriptor& descriptor, const void* data);

  std::unique_ptr<Future<AudioBufferHandle>>
  ui_add_in_memory(const AudioBufferDescriptor& descriptor, std::unique_ptr<unsigned char[]> data);

  std::unique_ptr<Future<RemoveResult>> ui_remove(AudioBufferHandle handle);

  Optional<AudioBufferChunk> render_get(AudioBufferHandle handle,
                                        uint64_t frame_begin,
                                        uint64_t frame_end) const;

  Optional<AudioBufferChunk>
  render_get(AudioBufferHandle handle, double frame_index, const AudioRenderInfo& info) const {
    return render_get(handle, uint64_t(frame_index), uint64_t(frame_index) + info.num_frames);
  }

  Optional<AudioBufferChunk> ui_load(AudioBufferHandle handle) const;

private:
  std::vector<Command> pending_ui_submit;
  std::vector<Command> pending_reader_swap;
  uint64_t next_buffer_handle_id{1};

  InMemoryBackingStore in_memory_backing_store;
  InMemoryAudioBuffers in_memory_audio_buffers;
  InMemoryAccessor in_memory_audio_buffer_accessor{in_memory_audio_buffers};
};

}