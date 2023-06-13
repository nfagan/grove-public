#include "Transport.hpp"
#include "arpeggio.hpp"
#include "grove/common/config.hpp"
#include "grove/common/common.hpp"
#include <vector>

GROVE_NAMESPACE_BEGIN

void Transport::set_bpm(double beats) {
  assert(beats > 0.0 && beats <= 1024.0);
  settable_bpm.store(beats);
}

void Transport::begin_render(const AudioRenderInfo& info) {
  update_play_commands();

  if (auto grid = read(&grid_handshake)) {
    render_canonical_grid = grid.value();
  }

  canonical_bpm = settable_bpm.load();
  ui_is_playing.store(is_playing);

  if (just_stopped()) {
    cursor_location.zero();

  } else if (just_played()) {
    cursor_location.zero();
    scheduling_info.zero();
    pausing_cursor_location.zero();
  }

  if (is_playing) {
    update_scheduling_quantum_info(info);
  }

  render_set_quantized_event_frame_offsets(pausing_cursor_location, info);

  process_block_size = ScoreCursor::from_beats(
    beats_per_sample(info.sample_rate) * double(info.num_frames),
    reference_time_signature().numerator);
}

void Transport::end_render(const AudioRenderInfo& info) {
  clear_play_state();

  const auto beat_incr = beats_per_sample(info.sample_rate) * info.num_frames;
  if (is_playing) {
    cursor_location.wrapped_add_beats(beat_incr, beats_per_measure());
  }

  pausing_cursor_location.wrapped_add_beats(beat_incr, beats_per_measure());
}

void Transport::update_play_commands() {
  //  Process at most one command per epoch.
  int num_process = std::min(1, play_commands.size());

  for (int i = 0; i < num_process; i++) {
    auto command = play_commands.read();
    if (command == PlayCommand::Stop && is_playing) {
      play_state = PlayState::stopped;
      is_playing = false;

    } else if (command == PlayCommand::Play && !is_playing) {
      play_state = PlayState::played;
      is_playing = true;

    } else if (command == PlayCommand::Toggle) {
      play_state = is_playing ? PlayState::stopped : PlayState::played;
      is_playing = !is_playing;
    }
  }
}

void Transport::clear_play_state() {
  play_state = 0;
}

double Transport::get_bpm() const {
  return canonical_bpm;
}

double Transport::beats_per_sample(double sample_rate) const {
  return beats_per_sample_at_bpm(canonical_bpm, sample_rate, reference_time_signature());
}

double Transport::beats_per_measure() const {
  return double(reference_time_signature().numerator);
}

void Transport::toggle_play_stop() {
  play_commands.maybe_write(PlayCommand::Toggle);
}

bool Transport::just_played() const {
  return play_state & PlayState::played;
}

bool Transport::just_stopped() const {
  return play_state & PlayState::stopped;
}

void Transport::update_scheduling_quantum_info(const AudioRenderInfo& info) {
  scheduling_info.next_quantum_render_frame_index_start = -1;
  scheduling_info.scheduling_error = ScoreCursor{};

  if (info.num_frames == 0) {
    return;
  }

  const double beats_per_meas = reference_time_signature().numerator;
  const auto beats_per_samp =
    beats_per_sample_at_bpm(canonical_bpm, info.sample_rate, reference_time_signature());
  const auto samp_per_beat = 1.0 / beats_per_samp;
  double beat_begin = cursor_location.to_beats(beats_per_meas);
  double beat_last = beats_per_samp * (info.num_frames - 1) + beat_begin;

  double quantum_beats = scheduling_quantum.to_beats(beats_per_meas);

  double begin_quantum = beat_begin / quantum_beats;
  double last_quantum = beat_last / quantum_beats;

  double begin_quantum_index = std::floor(begin_quantum);
  double last_quantum_index = std::floor(last_quantum);

  if (begin_quantum_index == begin_quantum &&
      begin_quantum_index != scheduling_info.current_quantum_index) {
    //  We start precisely on a quantum boundary.
    scheduling_info.next_quantum_render_frame_index_start = 0;
    scheduling_info.scheduling_error = ScoreCursor{}; //  0 error.

  } else if (begin_quantum_index != last_quantum_index) {
    scheduling_info.current_quantum_index = int64_t(last_quantum_index);

    //  The last frame in this render epoch lies in a new render epoch, meaning that, somewhere
    //  within (begin, last], we cross into the new epoch.
    double last_quantum_beat = last_quantum_index * quantum_beats;
    double render_relative_next_quantum_beat_begin = last_quantum_beat - beat_begin;
    double render_relative_next_quantum_frame_begin =
      render_relative_next_quantum_beat_begin * samp_per_beat;

    //  The next frame index start will be *before* the actual start time, after integer conversion.
    //  `beat_error` gives the amount of beats we have to offset cursors that reset to "zero" at
    //  `next_quantum_render_frame_index_start` in order to stay in sync.
    double frame_error = render_relative_next_quantum_frame_begin -
      std::floor(render_relative_next_quantum_frame_begin);
    double beat_error = frame_error * beats_per_samp;

    assert(render_relative_next_quantum_frame_begin >= 0 &&
           render_relative_next_quantum_frame_begin < info.num_frames);

    scheduling_info.next_quantum_render_frame_index_start =
      int(render_relative_next_quantum_frame_begin);

    scheduling_info.scheduling_error =
      ScoreCursor::from_beats(beat_error, beats_per_meas);
  }
}

void Transport::render_set_quantized_event_frame_offsets(
  const ScoreCursor& curs, const AudioRenderInfo& info) {
  //
  constexpr int num_quantums = int(audio::Quantization::QUANTIZATION_SIZE);
  std::fill(quantized_event_frame_offsets, quantized_event_frame_offsets + num_quantums, -1);

  if (info.num_frames == 0) {
    return;
  }

  const double tsig_num = reference_time_signature().numerator;
  const double bps = render_get_beats_per_sample(info.sample_rate);
  const double spb = 1.0 / bps;
  auto block_size = ScoreCursor::from_beats(bps * info.num_frames, tsig_num);
  ScoreRegion block{curs, block_size};

  for (int i = 0; i < num_quantums; i++) {
    auto quant = next_quantum(curs, audio::Quantization(i), tsig_num);
    if (block.contains(quant, tsig_num)) {
      assert(quant >= block.begin);
      quant.wrapped_sub_cursor(block.begin, tsig_num);
      double off = quant.to_sample_offset(spb, tsig_num);
      quantized_event_frame_offsets[i] = std::max(0, std::min(int(off), info.num_frames - 1));
    }
  }
}

void Transport::ui_update() {
  if (grid_handshake.awaiting_read) {
    if (acknowledged(&grid_handshake)) {
      ui_canonical_grid = grid_handshake.data;
    }
  }

  if (pending_canonical_grid && !grid_handshake.awaiting_read) {
    publish(&grid_handshake, std::move(pending_canonical_grid.value()));
    pending_canonical_grid = NullOpt{};
  }
}

void Transport::ui_set_grid(TransportGrid grid) {
  pending_canonical_grid = grid;
}

GROVE_NAMESPACE_END
