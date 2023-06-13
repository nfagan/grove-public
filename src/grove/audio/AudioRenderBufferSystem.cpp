#include "AudioRenderBufferSystem.hpp"
#include "grove/common/BuddyAllocator.hpp"
#include "grove/common/RingBuffer.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include <vector>
#include <unordered_map>

GROVE_NAMESPACE_BEGIN

using namespace audio_buffer_system;

namespace {

using DescriptorCountType = uint32_t;

using Allocator = BuddyAllocator<256, 10>;  //  @TODO: Experiment with this.
using AllocatorBlock = Allocator::Block;
constexpr uint64_t allocator_page_size = Allocator::page_size_bytes;

static_assert(sizeof(OpaqueAllocationRecord) >= sizeof(AllocatorBlock));
static_assert(std::is_trivial_v<AllocatorBlock>);
static_assert(std::is_trivial_v<OpaqueAllocationRecord>);

[[maybe_unused]] constexpr const char* logging_id() {
  return "AudioRenderBufferSystem";
}

struct PageRequest {
  int num_pages;
};

struct PageResponse {
  std::unique_ptr<unsigned char[]> data;
  size_t mem_size;
};

struct BufferAwaitingEventArray {
  AllocatorBlock elements;
  int num_elements;
};

struct RenderStats {
  int num_allocator_pages;
  size_t num_allocated_bytes;
  size_t num_reserved_bytes;
  size_t max_bytes_allocated_in_epoch;
  size_t max_bytes_requested_in_epoch;
};

struct HandoffWillFree {
  struct WillFree {
    void fill(const AllocatorBlock* block, int num_blocks) {
      assert(size <= capacity);
      if (capacity < num_blocks) {
        blocks = std::make_unique<AllocatorBlock[]>(num_blocks);
        capacity = num_blocks;
      }

      memcpy(blocks.get(), block, num_blocks * sizeof(AllocatorBlock));
      size = num_blocks;
    }

    std::unique_ptr<AllocatorBlock[]> blocks;
    int size{};
    int capacity{};
  };

  bool render_read(WillFree* wf) {
    if (published.load()) {
      published.store(false);
      *wf = std::move(will_free);
      return true;
    } else {
      return false;
    }
  }
  void render_return(WillFree&& wf) {
    will_free = std::move(wf);
    finished_reading.store(true);
  }

  bool ui_read() {
    assert(awaiting_return);
    if (finished_reading.load()) {
      finished_reading.store(false);
      awaiting_return = false;
      return true;
    } else {
      return false;
    }
  }
  void ui_submit() {
    assert(!awaiting_return && !finished_reading.load() && !published.load());
    awaiting_return = true;
    published.store(true);
  }

  bool awaiting_return{};
  std::atomic<bool> published{};
  std::atomic<bool> finished_reading{};
  WillFree will_free{};
};

struct AudioBufferSystem {
  Allocator render_allocator;
  size_t bytes_requested_this_epoch{};
  size_t bytes_allocated_this_epoch{};
  size_t max_bytes_allocated_in_epoch{};
  size_t max_bytes_requested_in_epoch{};

  RingBuffer<PageRequest, 32> page_requests;
  size_t pages_requested{};

  int num_queued_page_requests{};
  RingBuffer<PageResponse, 32> page_responses;
  RingBuffer<BufferAwaitingEventArray, 32> buffers_submitted_to_ui;

  HandoffWillFree handoff_will_free;

  std::vector<BufferAwaitingEvent> pending_wait;
  int num_pending_wait{};

  std::unordered_map<uint64_t, BufferAwaitingEvent> received;
  std::vector<AllocatorBlock> pending_free;

  std::vector<BufferAwaitingEvent> newly_received;
  uint64_t latest_ready_event_id{};

  RingBuffer<RenderStats, 32> render_stats;
  RenderStats ui_latest_render_stats{};
};

size_t size_of_channel(BufferChannelType type) {
  switch (type) {
    case BufferChannelType::Float: {
      return sizeof(float);
    }
    default: {
      assert(false);
      return 0;
    }
  }
}

size_t size_of_descriptor(uint32_t num_channels) {
  return sizeof(DescriptorCountType) + num_channels * sizeof(BufferChannelType);
}

size_t stride_of_frame(const BufferChannelType* channels, uint32_t num_channels) {
  size_t s{};
  for (uint32_t i = 0; i < num_channels; i++) {
    s += size_of_channel(channels[i]);
  }
  return s;
}

size_t allocation_size(const BufferChannelType* channels, uint32_t num_channels,
                       uint32_t num_frames) {
  return size_of_descriptor(num_channels) + stride_of_frame(channels, num_channels) * num_frames;
}

OpaqueAllocationRecord block_to_opaque_record(AllocatorBlock block) {
  OpaqueAllocationRecord result{};
  memcpy(result.data, &block, sizeof(AllocatorBlock));
  return result;
}

AllocatorBlock opaque_record_to_block(OpaqueAllocationRecord record) {
  AllocatorBlock result{};
  memcpy(&result, record.data, sizeof(AllocatorBlock));
  return result;
}

void write_descriptor(const BufferChannelType* channels, uint32_t num_channels, unsigned char* dst) {
  memcpy(dst, &num_channels, sizeof(uint32_t));
  dst += sizeof(uint32_t);
  for (uint32_t i = 0; i < num_channels; i++) {
    memcpy(dst + i * sizeof(BufferChannelType), &channels[i], sizeof(BufferChannelType));
  }
}

uint32_t read_num_channels(const unsigned char* data) {
  uint32_t r{};
  memcpy(&r, data, sizeof(uint32_t));
  return r;
}

unsigned char* read_data_ptr(unsigned char* data) {
  return data + sizeof(uint32_t) + sizeof(BufferChannelType) * read_num_channels(data);
}

BufferChannelType read_ith_channel_type(const unsigned char* data, uint32_t i) {
  assert(i < read_num_channels(data));
  BufferChannelType r{};
  memcpy(&r, data + sizeof(uint32_t) + i * sizeof(BufferChannelType), sizeof(BufferChannelType));
  return r;
}

void render_free(AudioBufferSystem* sys, BufferView view) {
  sys->render_allocator.free(opaque_record_to_block(view.alloc));
}

bool render_allocate(AudioBufferSystem* sys, const BufferChannelType* channels,
                     uint32_t num_channels, uint32_t num_frames, BufferView* dst) {
  const size_t alloc_size = allocation_size(channels, num_channels, num_frames);
  sys->bytes_requested_this_epoch += alloc_size;

  AllocatorBlock block{};
  if (sys->render_allocator.try_allocate(alloc_size, &block)) {
    write_descriptor(channels, num_channels, block.data);

    *dst = {};
    dst->size = alloc_size;
    dst->alloc = block_to_opaque_record(block);

    sys->bytes_allocated_this_epoch += alloc_size;
    return true;
  }

  return false;
}

void render_begin_process(AudioBufferSystem* sys) {
  {
    int num_responses = sys->page_responses.size();
    for (int i = 0; i < num_responses; i++) {
      auto resp = sys->page_responses.read();
      sys->render_allocator.push_page(std::move(resp.data), resp.mem_size);

      assert(sys->pages_requested > 0);
      sys->pages_requested--;
    }
  }
  {
    HandoffWillFree::WillFree wf;
    if (sys->handoff_will_free.render_read(&wf)) {
      for (int i = 0; i < wf.size; i++) {
        sys->render_allocator.free(wf.blocks[i]);
      }
      sys->handoff_will_free.render_return(std::move(wf));
    }
  }
}

void render_submit_pending_wait(AudioBufferSystem* sys) {
  if (sys->num_pending_wait == 0) {
    return;
  }

  bool need_release{};
  if (sys->buffers_submitted_to_ui.full()) {
    //  Wanted to deliver some buffers to the main thread, but couldn't, so discard these.
    GROVE_LOG_SEVERE_CAPTURE_META("Submit buffer full.", logging_id());
    need_release = true;
  } else {
    size_t wait_size = sizeof(BufferAwaitingEvent) * sys->num_pending_wait;
    sys->bytes_requested_this_epoch += wait_size;

    AllocatorBlock block{};
    if (sys->render_allocator.try_allocate(wait_size, &block)) {
      memcpy(block.data, sys->pending_wait.data(), wait_size);

      BufferAwaitingEventArray arr{};
      arr.elements = block;
      arr.num_elements = sys->num_pending_wait;

      sys->buffers_submitted_to_ui.write(arr);
      sys->bytes_allocated_this_epoch += wait_size;
    } else {
      //  Could have delivered some buffers to the main thread, but not enough space to hold
      //  the array of records. More pages will be requested later.
      GROVE_LOG_SEVERE_CAPTURE_META("Too many buffers pending.", logging_id());
      need_release = true;
    }
  }

  if (need_release) {
    for (int i = 0; i < sys->num_pending_wait; i++) {
      sys->render_allocator.free(opaque_record_to_block(sys->pending_wait[i].buff.alloc));
    }
  }

  sys->num_pending_wait = 0;
}

void render_submit_stats(AudioBufferSystem* sys) {
  if (sys->render_stats.full()) {
    return;
  }

  RenderStats stats{};
  stats.max_bytes_allocated_in_epoch = sys->max_bytes_allocated_in_epoch;
  stats.max_bytes_requested_in_epoch = sys->max_bytes_requested_in_epoch;
  stats.num_allocated_bytes = sys->render_allocator.bytes_allocated();
  stats.num_reserved_bytes = sys->render_allocator.bytes_reserved();
  stats.num_allocator_pages = int(sys->render_allocator.num_pages());
  sys->render_stats.write(stats);
}

void render_dispatch_page_requests(AudioBufferSystem* sys) {
  assert(sys->bytes_allocated_this_epoch <= sys->bytes_requested_this_epoch);
  size_t failed_to_allocate = sys->bytes_requested_this_epoch - sys->bytes_allocated_this_epoch;
  size_t num_required_pages = detail::u64_ceil_div(failed_to_allocate, allocator_page_size);

  if (num_required_pages > sys->pages_requested) {
    int num_req = int(num_required_pages - sys->pages_requested);
    assert(num_req < 1024);  //  arbitrary - don't expect a lot of requests in one epoch.
    PageRequest req{};
    req.num_pages = num_req;
    if (sys->page_requests.maybe_write(req)) {
      sys->pages_requested += num_req;
    } else {
      GROVE_LOG_SEVERE_CAPTURE_META("Page request buffer full.", logging_id());
    }
  }

  sys->max_bytes_allocated_in_epoch = std::max(
    sys->max_bytes_allocated_in_epoch, sys->bytes_allocated_this_epoch);
  sys->max_bytes_requested_in_epoch = std::max(
    sys->max_bytes_requested_in_epoch, sys->bytes_requested_this_epoch);

  sys->bytes_allocated_this_epoch = 0;
  sys->bytes_requested_this_epoch = 0;
}

void render_end_process(AudioBufferSystem* sys) {
  render_submit_pending_wait(sys);
  render_dispatch_page_requests(sys);
  render_submit_stats(sys);
}

void render_wait_for_event(AudioBufferSystem* sys, uint64_t event_id, uint32_t tag,
                           uint32_t instance, BufferView buff) {
  assert(event_id > 0);
  assert(tag > 0);
  assert(instance > 0);

  BufferAwaitingEvent awaiting{};
  awaiting.event_id = event_id;
  awaiting.type_tag = tag;
  awaiting.instance_id = instance;
  awaiting.buff = buff;

  if (sys->num_pending_wait == int(sys->pending_wait.size())) {
    //  @NOTE: Requires allocation.
    int new_sz = sys->num_pending_wait == 0 ? 512 : sys->num_pending_wait * 2;
    sys->pending_wait.resize(new_sz);
  }

  sys->pending_wait[sys->num_pending_wait++] = awaiting;
}

void ui_respond_to_page_requests(AudioBufferSystem* sys) {
  int num_reqs = sys->page_requests.size();
  for (int i = 0; i < num_reqs; i++) {
    auto req = sys->page_requests.read();
    sys->num_queued_page_requests += req.num_pages;
  }

  while (sys->num_queued_page_requests > 0) {
    PageResponse response{};
    response.mem_size = allocator_page_size;
    response.data = std::make_unique<unsigned char[]>(allocator_page_size);
    if (sys->page_responses.maybe_write(std::move(response))) {
      sys->num_queued_page_requests--;
    } else {
      break;
    }
  }
}

void ui_read_submitted(AudioBufferSystem* sys) {
  const int num_sent = sys->buffers_submitted_to_ui.size();
  for (int i = 0; i < num_sent; i++) {
    BufferAwaitingEventArray buff_array = sys->buffers_submitted_to_ui.read();

    for (int j = 0; j < buff_array.num_elements; j++) {
      BufferAwaitingEvent evt;
      memcpy(&evt, buff_array.elements.data + j * sizeof(evt), sizeof(evt));

      auto rcv_it = sys->received.find(evt.event_id);
      if (rcv_it != sys->received.end()) {
        //  Another buffer was already received with the same id, so free the existing one. This
        //  might not be an error, but it is suspicious.
        GROVE_LOG_SEVERE_CAPTURE_META("Duplicate event id.", logging_id());
        sys->pending_free.push_back(opaque_record_to_block(rcv_it->second.buff.alloc));
      }

      sys->received[evt.event_id] = evt;
    }

    sys->pending_free.push_back(buff_array.elements);
  }
}

void ui_submit_pending_free(AudioBufferSystem* sys) {
  auto& will_free = sys->handoff_will_free;
  if (will_free.awaiting_return) {
    (void) will_free.ui_read();
  }
  if (!sys->pending_free.empty() && !will_free.awaiting_return) {
    will_free.will_free.fill(sys->pending_free.data(), int(sys->pending_free.size()));
    will_free.ui_submit();
    sys->pending_free.clear();
  }
}

void ui_update_newly_received(AudioBufferSystem* sys, ArrayView<const uint32_t> newly_ready_events) {
  //  @NOTE: Make sure to call ui_submit_pending_free first, otherwise the newly ready buffers
  //  will be possibly freed before they are read.
  sys->newly_received.clear();
  for (const uint32_t evt : newly_ready_events) {
    sys->latest_ready_event_id = std::max(sys->latest_ready_event_id, uint64_t(evt));

    auto it = sys->received.find(evt);
    if (it != sys->received.end()) {
      BufferAwaitingEvent buff = it->second;
      sys->newly_received.push_back(buff);
      sys->pending_free.push_back(opaque_record_to_block(buff.buff.alloc));
      sys->received.erase(it);
    } else {
      GROVE_LOG_SEVERE_CAPTURE_META("No buffer received for event id.", logging_id());
    }
  }
}

void ui_drop_received(AudioBufferSystem* sys) {
  //  @NOTE: When an event is dropped, it will never be received by the main thread. Any received
  //  buffers waiting on this event id would then be stuck waiting and never freed. Here we naively
  //  just release all received buffers in that case.
  for (auto& [_, buff] : sys->received) {
    sys->pending_free.push_back(opaque_record_to_block(buff.buff.alloc));
  }
  sys->received.clear();
}

void ui_drop_expired(AudioBufferSystem* sys) {
  //  @TODO: This assumes event ids always increment and never overflow. Additionally, it assumes
  //  that events will be received in order.
  auto it = sys->received.begin();
  while (it != sys->received.end()) {
    auto& buff = it->second;
    if (buff.event_id < sys->latest_ready_event_id) {
      GROVE_LOG_SEVERE_CAPTURE_META(
        "Received buffer has en event id preceding the most recent ready event id.", logging_id());
      sys->pending_free.push_back(opaque_record_to_block(buff.buff.alloc));
      it = sys->received.erase(it);
    } else {
      ++it;
    }
  }
}

void ui_read_render_stats(AudioBufferSystem* sys) {
  const int num_stats = sys->render_stats.size();
  for (int i = 0; i < num_stats; i++) {
    sys->ui_latest_render_stats = sys->render_stats.read();
  }
}

void ui_update(AudioBufferSystem* sys, ArrayView<const uint32_t> newly_ready_events,
               bool dropped_some_events) {
  ui_respond_to_page_requests(sys);
  ui_read_submitted(sys);
  ui_submit_pending_free(sys);
  //  @NOTE: call ui_submit_pending_free before ui_update_newly_received
  ui_update_newly_received(sys, newly_ready_events);
  ui_drop_expired(sys);
  if (dropped_some_events) {
    ui_drop_received(sys);
  }
  ui_read_render_stats(sys);
}

ArrayView<const BufferAwaitingEvent> ui_read_newly_received(const AudioBufferSystem* sys) {
  return make_view(sys->newly_received);
}

void ui_terminate(AudioBufferSystem* sys) {
  sys->render_allocator.clear();
}

struct {
  AudioBufferSystem buffer_system;
} globals;

} //  anon

void audio_buffer_system::render_begin_process() {
  render_begin_process(&globals.buffer_system);
}

void audio_buffer_system::render_end_process() {
  render_end_process(&globals.buffer_system);
}

bool audio_buffer_system::render_allocate(const BufferChannelType* channels, uint32_t num_channels,
                                          uint32_t num_frames, BufferView* dst) {
  return render_allocate(&globals.buffer_system, channels, num_channels, num_frames, dst);
}

bool audio_buffer_system::render_allocate(BufferChannelType channel, uint32_t num_frames,
                                          BufferView* dst) {
  return render_allocate(&channel, 1, num_frames, dst);
}

void audio_buffer_system::render_free(BufferView view) {
  render_free(&globals.buffer_system, view);
}

void audio_buffer_system::render_wait_for_event(uint64_t event_id, uint32_t tag,
                                                uint32_t instance, BufferView buff) {
  render_wait_for_event(&globals.buffer_system, event_id, tag, instance, buff);
}

void audio_buffer_system::ui_update(const ArrayView<const uint32_t>& newly_ready_event_ids,
                                    bool dropped_some_events) {
  ui_update(&globals.buffer_system, newly_ready_event_ids, dropped_some_events);
}

ArrayView<const BufferAwaitingEvent> audio_buffer_system::ui_read_newly_received() {
  return ui_read_newly_received(&globals.buffer_system);
}

void audio_buffer_system::ui_terminate() {
  ui_terminate(&globals.buffer_system);
}

Stats audio_buffer_system::ui_get_stats() {
  auto* sys = &globals.buffer_system;
  Stats result{};
  result.allocator_page_size_bytes = allocator_page_size;
  result.num_allocator_pages = sys->ui_latest_render_stats.num_allocator_pages;
  result.num_allocated_bytes = sys->ui_latest_render_stats.num_allocated_bytes;
  result.num_reserved_bytes = sys->ui_latest_render_stats.num_reserved_bytes;
  result.max_bytes_allocated_in_epoch = sys->ui_latest_render_stats.max_bytes_allocated_in_epoch;
  result.max_bytes_requested_in_epoch = sys->ui_latest_render_stats.max_bytes_requested_in_epoch;

  result.num_received_buffers = int(sys->received.size());
  result.num_pending_free = int(sys->pending_free.size());
  return result;
}

unsigned char* audio_buffer_system::BufferView::data_ptr() const {
  if (size == 0) {
    return nullptr;
  } else {
    auto block = opaque_record_to_block(alloc);
    return read_data_ptr(block.data);
  }
}

void audio_buffer_system::BufferView::zero() {
  if (auto* ptr = data_ptr()) {
    auto data_size = frame_stride() * num_frames();
    if (data_size > 0) {
      assert(data_size + size_of_descriptor(num_channels()) == size);
      memset(ptr, 0, data_size);
    }
  }
}

uint32_t audio_buffer_system::BufferView::num_channels() const {
  if (size == 0) {
    return 0;
  } else {
    auto block = opaque_record_to_block(alloc);
    return read_num_channels(block.data);
  }
}

size_t audio_buffer_system::BufferView::frame_stride() const {
  if (size == 0) {
    return 0;
  }

  const auto block = opaque_record_to_block(alloc);
  const uint32_t num_chans = read_num_channels(block.data);
  assert(num_chans > 0);

  size_t stride{};
  for (uint32_t i = 0; i < num_chans; i++) {
    stride += size_of_channel(read_ith_channel_type(block.data, i));
  }
  assert(stride > 0);
  return stride;
}

uint32_t audio_buffer_system::BufferView::num_frames() const {
  if (size == 0) {
    return 0;
  }

  const size_t stride = frame_stride();
  const size_t desc_size = size_of_descriptor(num_channels());
  assert(size >= desc_size);

  auto rem_size = size - desc_size;
  assert((rem_size % stride) == 0);
  const auto result = uint32_t(rem_size / stride);
  return result;
}

BufferChannelType audio_buffer_system::BufferView::ith_channel_type(uint32_t i) const {
  assert(size > 0);
  auto block = opaque_record_to_block(alloc);
  return read_ith_channel_type(block.data, i);
}

bool audio_buffer_system::BufferView::is_float2() const {
  return num_channels() == 2 &&
         ith_channel_type(0) == BufferChannelType::Float &&
         ith_channel_type(1) == BufferChannelType::Float;
}

GROVE_NAMESPACE_END
