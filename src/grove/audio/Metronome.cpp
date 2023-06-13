#include "Metronome.hpp"
#include "oscillator.hpp"
#include "Transport.hpp"
#include "envelope.hpp"
#include "grove/common/common.hpp"
#include <atomic>

GROVE_NAMESPACE_BEGIN

namespace metronome {

struct Config {
  static constexpr float max_gain = 0.25f;
};

struct Metronome {
  std::atomic<bool> initialized{};
  const Transport* transport{};

  std::atomic<float> set_global_gain{Config::max_gain};
  bool disabled{};

  double osc_phase{};
  double osc_freq{frequency_a4()};
  env::ADSRExp<float> env;
  audio::ExpInterpolated<float> start_stop_gain{0.0f};
  audio::ExpInterpolated<float> global_gain{0.0f};
};

} //  metronome

namespace {

Envelope::Params make_env_params() {
  Envelope::Params params{};
  params.attack_time = 5e-3;
  params.decay_time = 0.125;
  params.sustain_time = 0.0;
  params.sustain_amp = 0.0;
  params.release_time = 0.0;
  params.infinite_sustain = false;
  return params;
}

struct {
  metronome::Metronome metronome;
} globals;

} //  anon

metronome::Metronome* metronome::get_global_metronome() {
  return &globals.metronome;
}

void metronome::ui_toggle_enabled(Metronome* metronome) {
  if (metronome->disabled) {
    ui_set_enabled(metronome, true);
  } else {
    ui_set_enabled(metronome, false);
  }
}

void metronome::ui_set_enabled(Metronome* metronome, bool enable) {
  if (enable) {
    metronome->set_global_gain.store(Config::max_gain);
    metronome->disabled = false;
  } else {
    metronome->set_global_gain.store(0.0f);
    metronome->disabled = true;
  }
}

bool metronome::ui_is_enabled(const Metronome* metronome) {
  return metronome->disabled;
}

void metronome::ui_initialize(Metronome* metronome, const Transport* transport) {
  assert(!metronome->initialized.load());
  metronome->env.configure(make_env_params());
  metronome->start_stop_gain.set_time_constant95(2e-3f);
  metronome->global_gain.set_time_constant95(2e-3f);
  metronome->transport = transport;
  metronome->initialized.store(true);
}

void metronome::render_process(Metronome* metronome, Sample* dst, const AudioRenderInfo& info) {
  if (!metronome->initialized.load()) {
    return;
  }

  metronome->global_gain.set_target(metronome->set_global_gain.load());

  const auto* transport = metronome->transport;
  const auto quant = audio::Quantization::Quarter;
  const int fi = transport->render_get_pausing_cursor_quantized_event_frame_offset(quant);
  const bool playing = transport->render_is_playing();

  if (transport->just_played()) {
    metronome->start_stop_gain.set_target(1.0f);
  } else if (transport->just_stopped()) {
    metronome->start_stop_gain.set_target(0.0f);
  }

  for (int i = 0; i < info.num_frames; i++) {
    double s = osc::Sin::tick(info.sample_rate, &metronome->osc_phase, metronome->osc_freq);
    if (playing && fi == i) {
      metronome->env.note_on();
    }

    const float start_stop_g = metronome->start_stop_gain.tick(float(info.sample_rate));
    const float global_g = metronome->global_gain.tick(float(info.sample_rate));
    const float g = metronome->env.tick(info.sample_rate);
    s = s * g * global_g * start_stop_g;

    for (int j = 0; j < info.num_channels; j++) {
      dst[i * info.num_channels + j] = float(s);
    }
  }
}

GROVE_NAMESPACE_END
