#pragma once

#include "grove/common/ArrayView.hpp"

namespace grove::audio_buffer_system {

struct Stats {
  int num_allocator_pages;
  size_t allocator_page_size_bytes;
  size_t num_allocated_bytes;
  size_t num_reserved_bytes;
  size_t max_bytes_allocated_in_epoch;
  size_t max_bytes_requested_in_epoch;

  int num_received_buffers;
  int num_pending_free;
};

enum class BufferChannelType : uint8_t {
  Float = 0,
};

struct OpaqueAllocationRecord {
  unsigned char data[16];  //  static_assert(sizeof(<actual record>) == sizeof(OpaqueAllocRecord))
};

struct BufferView {
  unsigned char* data_ptr() const;
  void zero();
  uint32_t num_channels() const;
  uint32_t num_frames() const;
  size_t frame_stride() const;
  BufferChannelType ith_channel_type(uint32_t i) const;
  bool is_float2() const;

  OpaqueAllocationRecord alloc;
  size_t size;
};

struct BufferAwaitingEvent {
  BufferView buff;
  uint64_t event_id;
  uint32_t type_tag;
  uint32_t instance_id;
};

void render_begin_process();
void render_end_process();
void render_wait_for_event(uint64_t event_id, uint32_t type, uint32_t instance, BufferView buff);
bool render_allocate(const BufferChannelType* channels, uint32_t num_channels, uint32_t num_frames,
                     BufferView* dst);
bool render_allocate(BufferChannelType channel, uint32_t num_frames, BufferView* dst);
void render_free(BufferView view);

void ui_update(const ArrayView<const uint32_t>& newly_ready_events, bool dropped_some_events);
ArrayView<const BufferAwaitingEvent> ui_read_newly_received();

void ui_terminate();
Stats ui_get_stats();

}