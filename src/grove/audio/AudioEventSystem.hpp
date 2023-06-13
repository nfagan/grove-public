#pragma once

#include "grove/common/identifier.hpp"
#include "grove/common/ArrayView.hpp"
#include "grove/common/Optional.hpp"
#include <cstdint>

#define GROVE_INCLUDE_NEW_EVENT_SYSTEM (1)

namespace grove {

struct AudioEvent;

struct AudioEventStreamHandle {
  GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
  uint32_t id;
};

struct AudioEventUpdateResult {
  ArrayView<const AudioEvent> newly_ready;
  ArrayView<const AudioEvent> newly_acquired;
};

} //  grove

namespace grove::audio_event_system {

struct Stats {
  uint32_t total_num_packets;
  uint32_t total_event_capacity;
  uint32_t max_packet_capacity;
  uint32_t latest_num_events_required;
  uint32_t num_pending_events;
  uint32_t num_newly_acquired_events;
  uint32_t num_newly_ready_events;
  float load_factor;
  float utilization;
};

void ui_initialize();
void ui_terminate();
[[nodiscard]] AudioEventUpdateResult ui_update(const Optional<double>& current_stream_time);
bool ui_check_dropped_events();
bool ui_check_render_buffer_overflow();
Stats ui_get_stats();

void render_begin_process();
void render_end_process(double output_buffer_dac_time, double sample_rate);
[[nodiscard]] bool render_push_event(AudioEventStreamHandle stream, const AudioEvent& event);
AudioEventStreamHandle default_event_stream();

}