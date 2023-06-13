#include "SpatiallyVaryingWind.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include "grove/common/Stopwatch.hpp"
#include "grove/common/Optional.hpp"
#include "grove/audio/envelope.hpp"
#include "grove/audio/oscillator.hpp"
#include "grove/math/random.hpp"
#include "grove/math/matrix_transform.hpp"

#define GROVE_SAMPLE_SLIME_MOLD (0)

#if GROVE_SAMPLE_SLIME_MOLD
#include "../generative/debug_slime_mold.hpp"
#endif

GROVE_NAMESPACE_BEGIN

/*
 * NewWindSystem
 */

class NewWindSystem {
public:
  static constexpr double peak_idle_amplitude = 0.2;
  static constexpr double gust_center_amplitude = 0.5;
  static constexpr double gust_amplitude_randomness_depth = 0.1;

  static constexpr double min_force() {
    return peak_idle_amplitude;
  }
  static constexpr double max_force() {
    return peak_idle_amplitude + gust_center_amplitude + gust_amplitude_randomness_depth;
  }

  enum class Regime {
    Breeze,
    Gust
  };

  enum class State {
    Idle,
    Gust,
    PendingFirstWave,
    PendingLastWave
  };

public:
  NewWindSystem() {
    Envelope::Params idle_low_params{};
    idle_low_params.attack_time = 1.0;
    idle_low_params.decay_time = 0.25;
    idle_low_params.infinite_sustain = true;
    idle_low_params.peak_amp = peak_idle_amplitude;
    idle_low_params.sustain_amp = peak_idle_amplitude;
    idle_low_params.release_time = 1.0;
    idle_low_envelope.configure(idle_low_params);
    idle_low_envelope.note_on();
  }

  float get_current_force() const {
    return current_force;
  }

public:
  env::ADSRLin<float> idle_low_envelope;
  State state{State::PendingFirstWave};
  Regime regime{Regime::Gust};

  float current_force{};
  bool first_entry{true};

  Optional<WindWavePlane::WaveID> gust_wave;
  Stopwatch regime_timer;
  double idle_state_duration{5.0};
};

/*
 * Impl
 */

namespace {

constexpr Vec3f world_wind_bounds_size() {
  return Vec3f{512.0f};
}

using WaveType = WindWavePlane::WaveType;
using WaveUpdate = WindWavePlane::UpdateResult;

bool is_idle_state(NewWindSystem::State state) {
  return state == NewWindSystem::State::Idle;
}

#if GROVE_LOGGING_ENABLED
constexpr const char* logging_id() {
  return "NewWindSystem";
}
#endif

inline Vec2f to_clamped_xz(const Vec2f& position_xz, const Bounds3f& wind_bounds) {
  Vec2f p0{wind_bounds.min.x, wind_bounds.min.z};
  Vec2f p1{wind_bounds.max.x, wind_bounds.max.z};
  return clamp_each((position_xz - p0) / (p1 - p0), Vec2f{}, Vec2f{1.0f});
}

void baseline_component(NewWindSystem& wind_system, WindWavePlane&,
                        const WaveUpdate&, const Vec2f&, double sr) {
  auto& env = wind_system.idle_low_envelope;
  wind_system.current_force = env.tick(float(sr));
}

void state_idle(NewWindSystem& wind_system, WindWavePlane&, const WaveUpdate&, const Vec2f&) {
  if (wind_system.first_entry) {
    GROVE_LOG_INFO_CAPTURE_META("Idle", logging_id());
    wind_system.regime_timer.reset();
    wind_system.first_entry = false;
    wind_system.idle_state_duration = 5.0 + urand() * 5.0;
  }

  if (wind_system.regime_timer.delta().count() > wind_system.idle_state_duration) {
    wind_system.state = NewWindSystem::State::PendingFirstWave;
    wind_system.first_entry = true;
  }
}

void state_pending_wave_offset(NewWindSystem& wind_system, WindWavePlane& wave_plane,
                               const WaveUpdate& update_res, const Vec2f&) {
  if (wind_system.first_entry) {
    GROVE_LOG_INFO_CAPTURE_META("End of wave.", logging_id());
    wind_system.first_entry = false;
    assert(wind_system.gust_wave);
    wave_plane.resume(wind_system.gust_wave.value());
  }

  for (auto& elapsed : update_res.elapsed_waves) {
    if (elapsed == wind_system.gust_wave.value()) {
      wind_system.state = NewWindSystem::State::Idle;
      wind_system.first_entry = true;
      break;
    }
  }
}

void state_pending_wave_onset(NewWindSystem& wind_system, WindWavePlane& wave_plane,
                              const WaveUpdate& update_res, const Vec2f& wind_dir) {
  if (wind_system.first_entry) {
    GROVE_LOG_INFO_CAPTURE_META("New wave.", logging_id());
    wind_system.first_entry = false;

    if (!wind_system.gust_wave) {
      auto wave = wave_plane.create_wave(wind_dir);
      wave.type = WaveType::TransientCosine;
      wave.center = 0.0f;
      wave.width = 0.1f;
      wave.amplitude = NewWindSystem::gust_center_amplitude;
      wave.incr = 0.0005f * 2.0f;
      wind_system.gust_wave = wave.id;
      wave_plane.push_wave(wave);

    } else {
      if (auto* wave = wave_plane.get_wave(wind_system.gust_wave.value())) {
        auto center = NewWindSystem::gust_center_amplitude;
        auto rand_span = NewWindSystem::gust_amplitude_randomness_depth;
        wave->amplitude = float(center + urand_11() * rand_span);
      }

      wave_plane.resume(wind_system.gust_wave.value());
    }
  }

  assert(wind_system.gust_wave);
  for (auto& elapsed : update_res.elapsed_waves) {
    if (elapsed == wind_system.gust_wave.value()) {
      wind_system.state = NewWindSystem::State::Gust;
      wind_system.first_entry = true;
      break;
    }
  }
}

void state_during_gust(NewWindSystem& wind_system, WindWavePlane&,
                       const WaveUpdate&, const Vec2f&) {
  if (wind_system.first_entry) {
    GROVE_LOG_INFO_CAPTURE_META("During gust.", logging_id());
    wind_system.first_entry = false;
    wind_system.regime_timer.reset();
  }

  if (wind_system.regime_timer.delta().count() > 10.0) {
    wind_system.state = NewWindSystem::State::PendingLastWave;
    wind_system.first_entry = true;
  }
}

void regime_gust(NewWindSystem& wind_system, WindWavePlane& wave_plane,
                 const WaveUpdate& update_res, const Vec2f& wind_dir) {
  using State = NewWindSystem::State;

  if (wind_system.state == State::PendingFirstWave) {
    state_pending_wave_onset(wind_system, wave_plane, update_res, wind_dir);

  } else if (wind_system.state == State::Gust) {
    state_during_gust(wind_system, wave_plane, update_res, wind_dir);

  } else if (wind_system.state == State::PendingLastWave) {
    state_pending_wave_offset(wind_system, wave_plane, update_res, wind_dir);

  } else if (wind_system.state == State::Idle) {
    state_idle(wind_system, wave_plane, update_res, wind_dir);
  }
}

Optional<NewWindSystem::State>
update_system(NewWindSystem& wind_system, WindWavePlane& wave_plane,
              const WaveUpdate& update_res, const Vec2f& wind_dir, double sr) {
  auto orig_state = wind_system.state;

  if (wind_system.regime == NewWindSystem::Regime::Gust) {
    regime_gust(wind_system, wave_plane, update_res, wind_dir);

  } else if (wind_system.regime == NewWindSystem::Regime::Breeze) {
    //
  }

  baseline_component(wind_system, wave_plane, update_res, wind_dir, sr);

  if (wind_system.state != orig_state) {
    return Optional<NewWindSystem::State>(wind_system.state);
  } else {
    return NullOpt{};
  }
}

} //  anon

SpatiallyVaryingWind::SpatiallyVaryingWind() :
  wind_bounds{-world_wind_bounds_size() * 0.5f, world_wind_bounds_size() * 0.5f},
  dominant_wind_direction{normalize(Vec2f{1.0f})},
  wind_system{new NewWindSystem()} {
  //
}

SpatiallyVaryingWind::~SpatiallyVaryingWind() {
  delete wind_system;
}

void SpatiallyVaryingWind::update_spectrum(const SpectrumAnalyzer::AnalysisFrame& frame) {
  spectral_influence.update(frame);
}

void SpatiallyVaryingWind::update_wind_direction_change(double real_dt, double sim_dt) {
  const auto theta_incr = float(sim_dt * 0.5 * (real_dt / sim_dt));

  if (theta_current < theta_target) {
    dominant_wind_direction = make_rotation(theta_current) * last_dominant_wind_direction;
    theta_current += theta_incr;
    if (theta_current >= theta_target) {
      theta_current = theta_target;
    }

    wave_plane.set_dominant_wind_direction(dominant_wind_direction);
  }
}

void SpatiallyVaryingWind::update(double real_dt) {
  const double sim_dt = 1.0 / 60.0;
  auto plane_update_res = wave_plane.update(real_dt, sim_dt);

  auto maybe_new_state = update_system(
    *wind_system, wave_plane, plane_update_res, dominant_wind_direction, 1.0 / real_dt);

  if (maybe_new_state && is_idle_state(maybe_new_state.value())) {
    last_dominant_wind_direction = dominant_wind_direction;
    theta_target = float(urand() * pi());
    theta_current = 0.0f;
  }

  update_wind_direction_change(real_dt, sim_dt);
  spectral_value = spectral_influence.current_value() * spectral_influence_strength;
}

void SpatiallyVaryingWind::set_dominant_wind_direction(const Vec2f& dir) {
  dominant_wind_direction = dir;
  wave_plane.set_dominant_wind_direction(dir);
}

Vec2f SpatiallyVaryingWind::wind_force(const Vec2f& position_xz) const {
  auto frac_p = to_clamped_xz(position_xz, wind_bounds);
  return wind_force_normalized_position(frac_p, 1.0f);
}

float SpatiallyVaryingWind::wind_force01_no_spectral_influence(const Vec2f& position_xz) const {
  auto p = to_clamped_xz(position_xz, wind_bounds);
  auto f = wind_force_normalized_position(p, 0.0f).length();
  auto min_f = NewWindSystem::min_force();
  auto max_f = NewWindSystem::max_force();
  return float(clamp((f - min_f) / (max_f - min_f), 0.0, 1.0));
}

Vec2f SpatiallyVaryingWind::wind_force_normalized_position(const Vec2f& p, float spect_scale) const {
  auto system_force = wind_system->get_current_force() * dominant_wind_direction;
  auto wave_force = wave_plane.evaluate_wave(p);
  auto spect_force = spectral_value * dominant_wind_direction * spect_scale;
#if GROVE_SAMPLE_SLIME_MOLD
  const auto slime_force3 = gen::sample_slime_mold01(p, 0.025f);
//  const auto slime_force = (slime_force3.x + slime_force3.y + slime_force3.z) / 3.0f;
  const auto slime_force = clamp(slime_force3.length(), 0.0f, 1.0f);
//  return lerp(0.25f, wave_force + system_force + spect_force, slime_force * dominant_wind_direction);
//  return wave_force + system_force + spect_force + slime_force * dominant_wind_direction;
  return slime_force * dominant_wind_direction;
#else
  return wave_force + system_force + spect_force;
#endif
}

Vec4f SpatiallyVaryingWind::world_bound_xz() const {
  auto bound = world_bound();
  return Vec4f{bound.min.x, bound.min.z, bound.max.x, bound.max.z};
}

Vec2f SpatiallyVaryingWind::to_normalized_position(const Vec2f& p) const {
  return to_clamped_xz(p, wind_bounds);
}

GROVE_NAMESPACE_END
