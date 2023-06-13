#include "AudioEventSystem.hpp"
#include "audio_events.hpp"
#include "grove/common/common.hpp"
#include "grove/common/RingBuffer.hpp"
#include "grove/common/Stopwatch.hpp"
#include "grove/common/profile.hpp"
#include <vector>
#include <memory>

GROVE_NAMESPACE_BEGIN

namespace {

constexpr double packet_load_factor_scale = 2.0;
constexpr double min_load_factor = 1.0;
constexpr double max_load_factor = 16.0;

constexpr uint32_t max_num_event_streams = 2;
constexpr uint32_t max_num_packets_per_allocator = 32;
constexpr uint32_t max_num_packets_per_stream_packet = 32;

constexpr uint32_t initial_events_per_packet = 4;
constexpr uint32_t max_num_events_per_packet = 4096;

constexpr float max_ui_delta_s = 48e-3f;
constexpr float ui_delta_lerp_factor = 0.01f;
constexpr float max_render_delta_s = 32e-3f;
constexpr float render_delta_lerp_factor = 0.025f;

struct SmoothedIntervalTimer {
  void reset() {
    first_update = true;
    iui = {};
  }

  Optional<float> update(float max_delta, float lerp_factor) {
    auto delta = stopwatch.delta_update();
    if (first_update) {
      first_update = false;
      return NullOpt{};
    } else {
      float delta_s = std::min(max_delta, float(delta.count()));
      iui = lerp(lerp_factor, iui, delta_s);
      return Optional<float>(iui);
    }
  }

  Stopwatch stopwatch;
  bool first_update{true};
  float iui{};
};

struct AudioEventPacket {
  void clear() {
    size = 0;
  }

  AudioEvent* events;
  uint32_t size;
  uint32_t capacity;
};

struct AudioEventPacketAllocator {
  void clear() {
    num_events_required = 0;
    num_events_acquired = 0;
    render_buffer_overflow = false;
    packet_index = 0;
    packet_capacity = 0;
    num_packets_requested = 0;
    total_num_packets_requested = 0;
    ui_request_packets.store(false);
    pending_packet_resize = false;
    num_awaiting_resize = 0;
    num_received_resize = 0;
    events_per_packet = 0;
  }

  uint32_t num_events_required;
  uint32_t num_events_acquired;
  bool render_buffer_overflow;
  AudioEventPacket* packets[max_num_packets_per_allocator];
  uint32_t packet_index;
  uint32_t packet_capacity;
  uint32_t num_packets_requested;
  uint32_t total_num_packets_requested;
  std::atomic<uint32_t> ui_request_packets;

  bool pending_packet_resize;
  uint32_t num_awaiting_resize;
  uint32_t num_received_resize;
  uint32_t events_per_packet;
};

struct AudioEventStreamEventPacket {
  void set_new() {
    flags |= uint8_t(1);
  }
  void set_was_aborted() {
    flags |= uint8_t(2);
  }
  void set_request_resize() {
    flags |= uint8_t(4);
  }
  void clear_request_resize() {
    flags &= ~(uint8_t(4));
  }
  void set_was_resized() {
    flags |= uint8_t(8);
  }
  bool is_new() const {
    return flags & uint8_t(1);
  }
  bool was_aborted() const {
    return flags & uint8_t(2);
  }
  bool request_resize() const {
    return flags & uint8_t(4);
  }
  bool was_resized() const {
    return flags & uint8_t(8);
  }

  AudioEventStreamHandle stream;
  AudioEventPacket* packet;
  uint8_t flags;
};

struct AudioEventStreamEventPackets {
  bool push(AudioEventStreamEventPacket packet) {
    assert(size < max_num_packets_per_stream_packet);
    packets[size++] = packet;
    return size == max_num_packets_per_stream_packet;
  }

  bool full() const {
    return size == max_num_packets_per_stream_packet;
  }

  AudioEventStreamEventPacket packets[max_num_packets_per_stream_packet];
  uint32_t size;
};

struct AudioEventStream {
  void clear() {
    handle = {};
    alloc.clear();
  }

  AudioEventStreamHandle handle{};
  AudioEventPacketAllocator alloc{};
};

struct UIAudioEvents {
  void clear() {
    *this = {};
  }

  std::vector<AudioEvent> pending_events;
  size_t pending_size{};

  std::vector<AudioEvent> newly_ready_events;
  size_t newly_ready_size{};

  std::vector<AudioEvent> newly_acquired_events;
  size_t newly_acquired_size{};
};

struct AudioEventSystem {
  void reset() {
    for (uint32_t s = 0; s < num_event_streams; s++) {
      streams[s].clear();
    }
    num_event_streams = 0;
    to_ui.clear();
    from_ui.clear();
    queued_from_ui.clear();
    dropped_events.store(false);
    render_buffer_overflow.store(false);
    latest_num_events_required.store(0);
    for (auto& packet : packet_store) {
      free(packet->events);
    }
    packet_store.clear();
    ui_events.clear();
    ui_timer.reset();
    render_timer.reset();
    packet_capacity_limit_reached = false;
  }

  AudioEventStream streams[max_num_event_streams];
  uint32_t num_event_streams;

  RingBuffer<AudioEventStreamEventPackets, 32> to_ui;
  RingBuffer<AudioEventStreamEventPackets, 32> from_ui;
  std::vector<AudioEventStreamEventPackets> queued_from_ui;

  std::atomic<bool> dropped_events{};
  std::atomic<bool> render_buffer_overflow{};
  std::atomic<uint32_t> latest_num_events_required{};
  std::vector<std::unique_ptr<AudioEventPacket>> packet_store;
  UIAudioEvents ui_events;

  std::atomic<float> ui_iui{};
  std::atomic<float> render_iui{};

  SmoothedIntervalTimer ui_timer;
  SmoothedIntervalTimer render_timer;

  bool packet_capacity_limit_reached{};
  bool initialized{};
};

int push_event(AudioEventPacket* packet, const AudioEvent& event) {
  assert(packet->size < packet->capacity);
  packet->events[packet->size++] = event;
  return packet->size == packet->capacity;
}

void set_output_time(AudioEventPacket* packet, double output_time, double sample_period) {
  for (uint32_t i = 0; i < packet->size; i++) {
    const uint64_t frame = packet->events[i].frame;
    packet->events[i].time = output_time + double(frame) * sample_period;
  }
}

void maybe_request_more_packets(AudioEventPacketAllocator* alloc, double load_factor) {
  if (alloc->pending_packet_resize) {
    //  Wait to finish a previously initiated resize round.
    return;
  }

  if (alloc->num_events_required == alloc->num_events_acquired) {
    //  No events were dropped.
    return;
  }

  //  The allocator should have enough packets to accommodate (load factor) * num packets
  const uint32_t num_required = alloc->num_events_required;
  const uint32_t events_per_packet = alloc->events_per_packet == 0 ?
    initial_events_per_packet : alloc->events_per_packet;

  const auto num_packets_required = uint32_t(
    std::ceil(double(num_required) / double(events_per_packet)));
  const auto num_packets_required_load = uint32_t(
    std::ceil(double(num_packets_required) * load_factor * packet_load_factor_scale));

  if (num_packets_required_load > alloc->total_num_packets_requested) {
    if (alloc->total_num_packets_requested < max_num_packets_per_allocator) {
      const uint32_t request_size = std::min(
        max_num_packets_per_allocator - alloc->total_num_packets_requested,
        num_packets_required_load - alloc->total_num_packets_requested);
      alloc->num_packets_requested += request_size;
      alloc->total_num_packets_requested += request_size;
      alloc->ui_request_packets += request_size;
    } else {
      //  initiate packet resize
      auto event_cap = alloc->total_num_packets_requested * events_per_packet;
      if (double(num_required) * load_factor * packet_load_factor_scale > event_cap) {
        alloc->pending_packet_resize = true;
        alloc->num_awaiting_resize = max_num_packets_per_allocator;
        alloc->num_received_resize = 0;
      }
    }
  }
}

bool push_event(AudioEventPacketAllocator* alloc, const AudioEvent& event) {
  alloc->num_events_required++;
  if (alloc->packet_index < alloc->packet_capacity) {
    auto* packet = alloc->packets[alloc->packet_index];
    alloc->packet_index += push_event(packet, event);
    alloc->num_events_acquired++;
    return true;
  } else {
    alloc->render_buffer_overflow = true;
    return false;
  }
}

uint32_t num_written_packets(AudioEventPacketAllocator* alloc) {
  uint32_t res = alloc->packet_index;
  if (alloc->packet_index < alloc->packet_capacity &&
      alloc->packets[alloc->packet_index]->size > 0) {
    res++;
  }
  return res;
}

void push_packet(AudioEventPacketAllocator* alloc, const AudioEventStreamEventPacket& packet) {
  assert(alloc->packet_capacity < max_num_packets_per_allocator);
  auto* dst_packet = packet.packet;
  alloc->packets[alloc->packet_capacity++] = dst_packet;

  if (packet.is_new()) {
    assert(alloc->num_packets_requested > 0);
    alloc->num_packets_requested--;
  }

  if (packet.was_resized()) {
    assert(!packet.is_new() && !packet.was_aborted() && !packet.request_resize());
    assert(alloc->pending_packet_resize);
    assert(alloc->num_received_resize < max_num_packets_per_allocator);

    alloc->num_received_resize++;
    if (alloc->num_received_resize == max_num_packets_per_allocator) {
      //  Finished resizing.
      assert(dst_packet->capacity > 0 && dst_packet->capacity >= alloc->events_per_packet);
      assert(alloc->num_awaiting_resize == 0);
      alloc->events_per_packet = dst_packet->capacity;
      alloc->pending_packet_resize = false;
#ifdef GROVE_DEBUG
      for (uint32_t i = 0; i < alloc->packet_capacity; i++) {
        assert(alloc->packets[i]->capacity == alloc->packets[0]->capacity);
      }
#endif
    }
  }

  if (packet.was_aborted()) {
    assert(!packet.is_new() && !packet.was_resized());
    if (packet.request_resize()) {
      assert(alloc->num_awaiting_resize < max_num_packets_per_allocator);
      alloc->num_awaiting_resize++;
    }
  }
}

AudioEventStreamEventPacket pop_packet(AudioEventPacketAllocator* alloc,
                                       AudioEventStreamHandle stream) {
  assert(alloc->packet_capacity > 0);
  auto* packet = alloc->packets[0];
  std::rotate(alloc->packets, alloc->packets + 1, alloc->packets + alloc->packet_capacity--);

  AudioEventStreamEventPacket event_packet{};
  event_packet.stream = stream;
  event_packet.packet = packet;
  if (alloc->pending_packet_resize && alloc->num_awaiting_resize > 0) {
    event_packet.set_request_resize();
    alloc->num_awaiting_resize--;
  }

  return event_packet;
}

void begin_process(AudioEventPacketAllocator* alloc) {
  alloc->packet_index = 0;
  alloc->num_events_required = 0;
  alloc->num_events_acquired = 0;
  alloc->render_buffer_overflow = false;
  for (uint32_t i = 0; i < alloc->packet_capacity; i++) {
    alloc->packets[i]->clear();
  }
}

bool ui_check_dropped_events(AudioEventSystem* event_system) {
  bool expect{true};
  return event_system->dropped_events.compare_exchange_strong(expect, false);
}

bool ui_check_render_buffer_overflow(AudioEventSystem* event_system) {
  bool expect{true};
  return event_system->render_buffer_overflow.compare_exchange_strong(expect, false);
}

AudioEventStream* get_stream(AudioEventSystem* event_system, AudioEventStreamHandle stream) {
  assert(stream.is_valid() && stream.id - 1 < max_num_event_streams);
  return &event_system->streams[stream.id - 1];
}

AudioEventPacketAllocator* get_allocator(AudioEventSystem* event_system,
                                         AudioEventStreamHandle stream) {
  return &get_stream(event_system, stream)->alloc;
}

void begin_process(AudioEventSystem* event_system) {
  const int num_incoming = event_system->from_ui.size();
  for (int i = 0; i < num_incoming; i++) {
    AudioEventStreamEventPackets stream_packets = event_system->from_ui.read();
    assert(stream_packets.size > 0);

    for (uint32_t j = 0; j < stream_packets.size; j++) {
      auto& stream_packet = stream_packets.packets[j];
      auto* stream = get_stream(event_system, stream_packet.stream);
      push_packet(&stream->alloc, stream_packet);
    }
  }

  for (uint32_t s = 0; s < event_system->num_event_streams; s++) {
    auto& stream = event_system->streams[s];
    begin_process(&stream.alloc);
  }
}

double compute_load_factor(AudioEventSystem* event_system, double min, double max) {
  float ui = event_system->ui_iui.load();
  float render = event_system->render_iui.load();
  double load = render > 0.0 ? ui / render : 0.0;
  return std::max(min, std::min(max, load));
}

void return_aborted_packets(AudioEventSystem* event_system, AudioEventStreamEventPackets* packets) {
  for (uint32_t i = 0; i < packets->size; i++) {
    auto& dst_packet = packets->packets[i];
    dst_packet.set_was_aborted();
    push_packet(get_allocator(event_system, dst_packet.stream), dst_packet);
  }
}

bool submit_packets(AudioEventSystem* event_system, double output_time, double sample_period) {
  AudioEventStreamEventPackets packets;
  packets.size = 0;

  bool discarded_events{};
  uint32_t total_num_events_required{};

  for (uint32_t s = 0; s < event_system->num_event_streams; s++) {
    auto& stream = event_system->streams[s];
    assert(stream.alloc.packet_index <= stream.alloc.packet_capacity);
    const uint32_t num_written = num_written_packets(&stream.alloc);
    total_num_events_required += stream.alloc.num_events_required;

    for (uint32_t i = 0; i < num_written; i++) {
      auto event_packet = pop_packet(&stream.alloc, stream.handle);
      set_output_time(event_packet.packet, output_time, sample_period);

      if (packets.push(event_packet)) {
        if (!event_system->to_ui.maybe_write(packets)) {
          return_aborted_packets(event_system, &packets);
          discarded_events = true;
        }
        packets.size = 0;
        if (discarded_events) {
          break;
        }
      }
    }
    if (discarded_events) {
      break;
    }
  }

  if (packets.size > 0) {
    if (!event_system->to_ui.maybe_write(packets)) {
      return_aborted_packets(event_system, &packets);
      discarded_events = true;
    }
    packets.size = 0;
  }

  if (discarded_events) {
    event_system->dropped_events.store(true);
  }

  event_system->latest_num_events_required.store(total_num_events_required);
  return discarded_events;
}

void end_process(AudioEventSystem* event_system, double output_time, double sample_rate) {
  if (auto iui = event_system->render_timer.update(max_render_delta_s, render_delta_lerp_factor)) {
    event_system->render_iui.store(iui.value());
  }

  const double load_factor = compute_load_factor(event_system, min_load_factor, max_load_factor);
  const bool discarded_events = submit_packets(event_system, output_time, 1.0 / sample_rate);

  bool render_buffer_overflow{};
  for (uint32_t s = 0; s < event_system->num_event_streams; s++) {
    auto* alloc = &event_system->streams[s].alloc;
    render_buffer_overflow = render_buffer_overflow || alloc->render_buffer_overflow;
    if (!discarded_events) {
      maybe_request_more_packets(alloc, load_factor);
    }
  }

  if (render_buffer_overflow) {
    event_system->render_buffer_overflow.store(true);
  }
}

void append_packet(std::vector<AudioEvent>& events, size_t* event_size,
                   const AudioEventPacket* packet) {
  size_t dst = *event_size;
  size_t dst_size = dst + packet->size;
  if (dst_size > events.size()) {
    events.resize(dst_size);
  }
  std::copy(packet->events, packet->events + packet->size, events.data() + dst);
  *event_size = dst_size;
}

void ui_push_newly_acquired(UIAudioEvents* events, const AudioEventPacket* packet) {
  append_packet(events->pending_events, &events->pending_size, packet);
  append_packet(events->newly_acquired_events, &events->newly_acquired_size, packet);
}

void ui_push_newly_ready(UIAudioEvents* ui_events, const AudioEvent* beg, const AudioEvent* end) {
  assert(ui_events->newly_ready_size <= uint32_t(ui_events->newly_ready_events.size()));

  auto& evts = ui_events->newly_ready_events;
  const auto num_add = uint32_t(end - beg);
  const auto curr_size = uint32_t(ui_events->newly_ready_size);
  const auto cap = uint32_t(evts.size());

  if (curr_size + num_add > cap) {
    evts.resize(curr_size + num_add);
  }
  if (num_add > 0) {
    std::copy(beg, end, evts.data() + curr_size);
    ui_events->newly_ready_size += num_add;
  }
}

void ui_clear_new_events(UIAudioEvents* ui_events) {
  ui_events->newly_ready_size = 0;
  ui_events->newly_acquired_size = 0;
}

AudioEventUpdateResult ui_update_events(UIAudioEvents* ui_events, double current_stream_time) {
  auto profiler = GROVE_PROFILE_SCOPE_TIC_TOC("AudioEventSystem/ui_update_events");
  (void) profiler;

  {
    auto less_by_time = [](const AudioEvent& a, const AudioEvent& b) { return a.time < b.time; };
    auto& pend = ui_events->pending_events;
    auto* beg = pend.data();
    auto* end = pend.data() + ui_events->pending_size;
    std::sort(beg, end, less_by_time);

    auto* it = beg;
    for (; it != end; ++it) {
      if (it->time > current_stream_time) {
        break;
      }
    }

    auto it_beg = pend.begin() + (beg - pend.data());
    auto it_it = pend.begin() + (it - pend.data());
    auto num_newly_ready = uint32_t(it - beg);

    ui_push_newly_ready(ui_events, beg, it);
    pend.erase(it_beg, it_it);
    ui_events->pending_size -= num_newly_ready;
  }

  AudioEventUpdateResult result{};
  result.newly_ready = {
    ui_events->newly_ready_events.data(),
    ui_events->newly_ready_events.data() + ui_events->newly_ready_size};
  result.newly_acquired = {
    ui_events->newly_acquired_events.data(),
    ui_events->newly_acquired_events.data() + ui_events->newly_acquired_size};
  return result;
}

void ui_respond_to_packet_requests(AudioEventSystem* event_system) {
  AudioEventStreamEventPackets packets;
  packets.size = 0;

  for (uint32_t s = 0; s < event_system->num_event_streams; s++) {
    auto& stream = event_system->streams[s];
    const uint32_t num_create = stream.alloc.ui_request_packets.load();
    stream.alloc.ui_request_packets -= num_create;

    for (uint32_t i = 0; i < num_create; i++) {
      auto& packet = event_system->packet_store.emplace_back();

      packet = std::make_unique<AudioEventPacket>();
      packet->events = (AudioEvent*) malloc(sizeof(AudioEvent) * initial_events_per_packet);
      assert(packet->events);
      packet->capacity = initial_events_per_packet;

      AudioEventStreamEventPacket new_packet{};
      new_packet.packet = packet.get();
      new_packet.stream = event_system->streams[s].handle;
      new_packet.set_new();

      if (packets.push(new_packet)) {
        if (!event_system->from_ui.maybe_write(packets)) {
          event_system->queued_from_ui.push_back(packets);
        }
        packets.size = 0;
      }
    }
  }

  if (packets.size > 0) {
    if (!event_system->from_ui.maybe_write(packets)) {
      event_system->queued_from_ui.push_back(packets);
    }
    packets.size = 0;
  }
}

void ui_read_packets(AudioEventSystem* event_system) {
  AudioEventStreamEventPackets dst_packets;
  dst_packets.size = 0;

  const int num_packets_to_read = event_system->to_ui.size();
  for (int i = 0; i < num_packets_to_read; i++) {
    AudioEventStreamEventPackets src_packets = event_system->to_ui.read();
    assert(src_packets.size > 0);

    for (uint32_t j = 0; j < src_packets.size; j++) {
      auto& stream_packet = src_packets.packets[j];
      assert(stream_packet.packet->size > 0 && !stream_packet.is_new());
      ui_push_newly_acquired(&event_system->ui_events, stream_packet.packet);

      if (stream_packet.request_resize()) {
        assert(!stream_packet.is_new() &&
               !stream_packet.was_aborted() &&
               !stream_packet.was_resized());

        auto potential_cap = stream_packet.packet->capacity * 2;
        if (potential_cap > max_num_events_per_packet) {
          event_system->packet_capacity_limit_reached = true;
        } else {
          auto new_cap = potential_cap;
          assert(stream_packet.packet->events);
          free(stream_packet.packet->events);
          stream_packet.packet->events = (AudioEvent*) malloc(new_cap * sizeof(AudioEvent));
          stream_packet.packet->capacity = new_cap;
        }
        //  @TODO: Should probably distinguish between "was successfully resized" and
        //   "didn't actually resize because the maximum capacity was reached."
        stream_packet.set_was_resized();
        stream_packet.clear_request_resize();
      }

      if (dst_packets.push(stream_packet)) {
        if (!event_system->from_ui.maybe_write(dst_packets)) {
          event_system->queued_from_ui.push_back(dst_packets);
        }
        dst_packets.size = 0;
      }
    }
  }

  if (dst_packets.size > 0) {
    if (!event_system->from_ui.maybe_write(dst_packets)) {
      event_system->queued_from_ui.push_back(dst_packets);
    }
  }
}

void ui_push_queued(AudioEventSystem* event_system) {
  auto it = event_system->queued_from_ui.begin();
  while (it != event_system->queued_from_ui.end()) {
    if (event_system->from_ui.maybe_write(*it)) {
      it = event_system->queued_from_ui.erase(it);
    } else {
      ++it;
    }
  }
}

AudioEventUpdateResult ui_update(AudioEventSystem* event_system, double current_stream_time) {
  if (auto iui = event_system->ui_timer.update(max_ui_delta_s, ui_delta_lerp_factor)) {
    event_system->ui_iui.store(iui.value());
  }

  ui_clear_new_events(&event_system->ui_events);
  ui_push_queued(event_system);
  ui_respond_to_packet_requests(event_system);
  ui_read_packets(event_system);
  return ui_update_events(&event_system->ui_events, current_stream_time);
}

void ui_initialize(AudioEventSystem* event_system, uint32_t n) {
  assert(!event_system->initialized);
  assert(n <= max_num_event_streams);
  for (uint32_t i = 0; i < n; i++) {
    event_system->streams[i].handle.id = i + 1;
  }
  event_system->num_event_streams = n;
  event_system->initialized = true;
}

void ui_terminate(AudioEventSystem* event_system) {
  if (!event_system->initialized) {
    return;
  }
  event_system->reset();
  event_system->initialized = false;
}

audio_event_system::Stats ui_get_stats(AudioEventSystem* event_system) {
  audio_event_system::Stats result{};
  result.total_num_packets = uint32_t(event_system->packet_store.size());

  for (auto& packet : event_system->packet_store) {
    result.total_event_capacity += packet->capacity;
    result.max_packet_capacity = std::max(result.max_packet_capacity, packet->capacity);
  }

  result.latest_num_events_required = event_system->latest_num_events_required.load();
  result.utilization = result.total_event_capacity == 0 ?
    0.0f : float(result.latest_num_events_required) / float(result.total_event_capacity);
  result.num_pending_events = uint32_t(event_system->ui_events.pending_size);
  result.num_newly_acquired_events = uint32_t(event_system->ui_events.newly_acquired_size);
  result.num_newly_ready_events = uint32_t(event_system->ui_events.newly_ready_size);

  {
    float ui = event_system->ui_iui.load();
    float render = event_system->render_iui.load();
    float load = render > 0.0f ? ui / render : 0.0f;
    result.load_factor = load;
  }

  return result;
}

AudioEventSystem global_event_system{};

} //  anon

void audio_event_system::render_begin_process() {
  begin_process(&global_event_system);
}

void audio_event_system::render_end_process(double output_buffer_dac_time, double sample_rate) {
  end_process(&global_event_system, output_buffer_dac_time, sample_rate);
}

bool audio_event_system::render_push_event(AudioEventStreamHandle stream, const AudioEvent& event) {
  return push_event(get_allocator(&global_event_system, stream), event);
}

AudioEventUpdateResult audio_event_system::ui_update(const Optional<double>& time) {
  const double time_value = time ? time.value() : -1.0;
  return ui_update(&global_event_system, time_value);
}

bool audio_event_system::ui_check_dropped_events() {
  return ui_check_dropped_events(&global_event_system);
}

bool audio_event_system::ui_check_render_buffer_overflow() {
  return ui_check_render_buffer_overflow(&global_event_system);
}

audio_event_system::Stats audio_event_system::ui_get_stats() {
  return ui_get_stats(&global_event_system);
}

void audio_event_system::ui_initialize() {
  ui_initialize(&global_event_system, 2);
}

void audio_event_system::ui_terminate() {
  ui_terminate(&global_event_system);
}

AudioEventStreamHandle audio_event_system::default_event_stream() {
  auto handle = global_event_system.streams[0].handle;
  assert(handle.is_valid());
  return handle;
}

GROVE_NAMESPACE_END
