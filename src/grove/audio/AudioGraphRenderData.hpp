#pragma once

#include "DoubleBuffer.hpp"
#include "data_channel.hpp"
#include <vector>
#include <memory>

namespace grove {

class AudioProcessorNode;
class AudioGraph;

/*
 * AudioMemoryArenas
 */

struct AudioMemoryArenas {
public:
  AudioMemoryArena* require();
  void make_all_available();

public:
  std::vector<std::unique_ptr<AudioMemoryArena>> arenas;
  std::vector<int> free_list;
};

/*
 * AudioGraphRenderData
 */

struct AudioGraphRenderData {
public:
  struct AllocInfo {
    BufferChannelSet<16> channel_set;
    AudioProcessBuffer buffer{};
    AudioMemoryArena* arena{};
  };

  struct ReadyToRender {
    AudioProcessorNode* node{};
    int output_buffer_index{};
    int input_buffer_index{};
    AudioProcessData input;
    AudioProcessData output;
    bool requires_allocation{false};
  };

public:
  static AudioGraphRenderData build(const AudioGraph& graph,
                                    AudioMemoryArenas& arenas,
                                    int num_frames);

public:
  std::vector<ReadyToRender> ready_to_render;
  std::vector<AllocInfo> alloc_info;
};

/*
 * AudioGraphDoubleBuffer
 */

class AudioGraphDoubleBuffer {
public:
  using BufferedRenderData = audio::DoubleBuffer<AudioGraphRenderData>;

  struct AccessorTraits {
    static constexpr bool enable_mutable_read() {
      return true;
    }

    static void modify(AudioGraphRenderData& data,
                       const AudioGraph& graph,
                       AudioMemoryArenas& arenas,
                       int reserve_frames);

    static inline AudioGraphRenderData* on_reader_swap(AudioGraphRenderData* write_to,
                                                       AudioGraphRenderData*) {
      return write_to;
    }
  };

  using Accessor = audio::DoubleBufferAccessor<AudioGraphRenderData, AccessorTraits>;

public:
  bool can_modify() const;
  void modify(const AudioGraph& graph, int reserve_frames);
  Accessor::WriterUpdateResult update();

  AudioGraphRenderData& maybe_swap_and_read() noexcept {
    return render_data_accessor.maybe_swap_and_read_mut();
  }

private:
  BufferedRenderData render_data;
  Accessor render_data_accessor{render_data};

  AudioMemoryArenas arenas0;
  AudioMemoryArenas arenas1;

  AudioMemoryArenas* write_arenas{&arenas0};
  AudioMemoryArenas* read_arenas{&arenas1};
};

}