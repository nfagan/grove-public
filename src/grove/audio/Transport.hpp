#pragma once

#include "types.hpp"
#include "grove/common/RingBuffer.hpp"
#include "grove/common/Handshake.hpp"
#include <atomic>

namespace grove {

struct TransportGrid {
  TimeSignature time_signature{4, 4};
};

class Transport {
public:
  enum class PlayCommand {
    None,
    Play,
    Stop,
    Toggle
  };

  struct PlayState {
    static constexpr unsigned int played = 1u;
    static constexpr unsigned int paused = 1u << 1u;
    static constexpr unsigned int stopped = 1u << 2u;
  };

  struct SchedulingInfo {
    bool render_period_has_new_quantum() const {
      return next_quantum_render_frame_index_start != -1;
    }
    void zero() {
      current_quantum_index = -1;
    }

    int next_quantum_render_frame_index_start{-1};
    ScoreCursor scheduling_error{};
    int64_t current_quantum_index{-1};
  };

public:
  void begin_render(const AudioRenderInfo& info);
  void end_render(const AudioRenderInfo& info);

  void set_bpm(double beats);
  double get_bpm() const;

  //  This cursor resets to zero when the transport is stopped (when previously it was playing)
  //  or when it begins playing (when previously it was stopped).
  ScoreCursor render_get_cursor_location() const {
    return cursor_location;
  }

  //  This cursor continues to increment when the transport is stopped, but resets to zero (like
  //  the "regular" cursor location) when play is resumed.
  ScoreCursor render_get_pausing_cursor_location() const {
    return pausing_cursor_location;
  }

  ScoreCursor render_get_scheduling_quantum() const {
    return scheduling_quantum;
  }

  ScoreCursor render_get_process_block_size() const {
    return process_block_size;
  }

  const SchedulingInfo& render_get_scheduling_info() const {
    return scheduling_info;
  }
  bool render_is_playing() const {
    return is_playing;
  }

  double render_get_beats_per_sample(double sample_rate) const {
    return beats_per_sample(sample_rate);
  }

  //  Returns a value >= 0 corresponding to the frame at which an event quantized to `quant` should
  //  begin, relative to the current processing block, if such an event occurs within the current
  //  block. Otherwise, returns -1.
  int render_get_pausing_cursor_quantized_event_frame_offset(audio::Quantization quant) const {
    return quantized_event_frame_offsets[int(quant)];
  }

  void toggle_play_stop();

  bool just_played() const;
  bool just_stopped() const;

  bool ui_playing() const {
    return ui_is_playing.load();
  }

  void ui_update();
  void ui_set_grid(TransportGrid grid);
  TransportGrid ui_get_canonical_grid() const {
    return ui_canonical_grid;
  }

private:
  void update_scheduling_quantum_info(const AudioRenderInfo& info);

  double beats_per_sample(double sample_rate) const;
  double beats_per_measure() const;

  void clear_play_state();
  void update_play_commands();

  void render_set_quantized_event_frame_offsets(const ScoreCursor& curs, const AudioRenderInfo& info);

private:
  double canonical_bpm{120.0};
  std::atomic<double> settable_bpm{120.0};

  TransportGrid render_canonical_grid{};
  TransportGrid ui_canonical_grid{};
  Optional<TransportGrid> pending_canonical_grid;
  Handshake<TransportGrid> grid_handshake;

  ScoreCursor cursor_location{};
  ScoreCursor process_block_size{};
  ScoreCursor scheduling_quantum{1, 0.0};
  SchedulingInfo scheduling_info;
  ScoreCursor pausing_cursor_location{};

  RingBuffer<PlayCommand, 4> play_commands;
  unsigned int play_state{};
  bool is_playing{false};
  std::atomic<bool> ui_is_playing{false};

  int quantized_event_frame_offsets[int(audio::Quantization::QUANTIZATION_SIZE)];
};

}