#include "WindWavePlane.hpp"
#include "grove/common/profile.hpp"
#include "grove/common/common.hpp"
#include "grove/math/constants.hpp"
#include "grove/math/util.hpp"
#include "grove/common/logging.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

inline Mat2f wind_direction_to_inverse_matrix(const Vec2f& dir) {
  auto x = dir;
  auto z = dir;
  z.y = z.x;
  z.x = -dir.y;
  return inverse(Mat2f{x, z} * Mat2f{std::sqrt(2.0f)});
}

using WindWave = WindWavePlane::WindWave;
using Strength = WindWavePlane::Strength;

bool transient_cosine_wave_update(WindWave& wave, Strength& strength, int dim) {
  auto w_min = wave.center - wave.width * 0.5;
  auto w_max = wave.center + wave.width * 0.5;

  for (int i = 0; i < dim; i++) {
    for (int j = 0; j < dim; j++) {
      auto ind = (i * dim) + j;

      auto py = float(i) / float(dim);
      auto px = float(j) / float(dim);
      Vec2f frac_p{px, py};
      auto p_half = frac_p - 0.5f;

      auto p_sample = wave.inv_m * p_half + 0.5f;
      auto p = p_sample.x;
      auto after_wave = wave.amplitude * wave.dir;

      if (p >= w_min && p < w_max) {
        auto fp = (p - w_min) / (w_max - w_min);
        if (fp > 0.5f) {
          auto pi2_p = (pif() * fp) - pi_over_two();
          auto h = std::cos(pi2_p) * wave.amplitude;
          strength[ind] += float(h) * wave.dir;
        } else {
          strength[ind] += after_wave;
        }

      } else if (p < w_min) {
        strength[ind] += after_wave;
      }
    }
  }

  bool just_elapsed = false;

  if (!wave.elapsed) {
    wave.center += wave.incr;

    if ((wave.incr >= 0.0f && wave.center >= 1.0f) ||
        (wave.incr < 0.0f && w_max < 0.0f)) {
      wave.elapsed = true;
      wave.incr = -wave.incr;
      just_elapsed = true;
    }
  }

  return just_elapsed;
}

bool traveling_cosine_wave_update(WindWave& wave, Strength& strength, int dim) {
  for (int i = 0; i < dim; i++) {
    for (int j = 0; j < dim; j++) {
      auto ind = (i * dim) + j;

      auto py = float(i) / float(dim);
      auto px = float(j) / float(dim);
      Vec2f frac_p{px, py};
      auto p_half = frac_p - 0.5f;

      auto p_sample = wave.inv_m * p_half + 0.5f;
      auto p = p_sample.x + wave.center;
      auto phase = p * wave.width * two_pi();
      auto h = (std::cos(phase) * 0.5 + 0.5) * wave.amplitude;
      strength[ind] += float(h) * wave.dir;
    }
  }

  wave.center += wave.incr;
  wave.center -= std::floor(wave.center);

  return false;
}

bool hump_wave_update(WindWave& wave, Strength& strength, int dim) {
  auto w_min = wave.center - wave.width * 0.5;
  auto w_max = wave.center + wave.width * 0.5;

  for (int i = 0; i < dim; i++) {
    for (int j = 0; j < dim; j++) {
      auto ind = (i * dim) + j;

      auto py = float(i) / float(dim);
      auto px = float(j) / float(dim);
      Vec2f frac_p{px, py};
      auto p_half = frac_p - 0.5f;

      auto p_sample = wave.inv_m * p_half + 0.5f;
      auto p = p_sample.x;

      if (p >= w_min && p < w_max) {
        auto fp = (p - w_min) / (w_max - w_min);
        auto pi2_p = (pif() * fp) - pi_over_two();
        auto h = std::cos(pi2_p) * wave.amplitude;
        strength[ind] += float(h) * wave.dir;
      }
    }
  }

  bool just_elapsed = false;

  if (!wave.elapsed) {
    wave.center += wave.incr;

    if ((wave.incr >= 0.0f && w_min >= 1.0f) ||
        (wave.incr < 0.0f && w_max < 0.0f)) {
      wave.elapsed = true;
      just_elapsed = true;
    }
  }

  return just_elapsed;
}

} //  anon

void WindWavePlane::set_dominant_wind_direction(const Vec2f& dir) {
  for (auto& wave : waves) {
    wave.dir = dir;
    wave.inv_m = wind_direction_to_inverse_matrix(dir);
  }
}

Vec2f WindWavePlane::evaluate_wave(const Vec2f& frac_p) const {
  int x = clamp(int(frac_p.x * float(dim)), 0, dim-1);
  int y = clamp(int(frac_p.y * float(dim)), 0, dim-1);
  auto ind = x * dim + y;
  auto last = clamp_each(strength_last[ind], Vec2f{-1.0f}, Vec2f{1.0f});
  auto curr = clamp_each(strength_curr[ind], Vec2f{-1.0f}, Vec2f{1.0f});
  return lerp(float(time_alpha), last, curr);
}

WindWavePlane::UpdateResult WindWavePlane::update(double real_dt, double sim_dt) {
  auto profiler = GROVE_PROFILE_SCOPE_TIC_TOC("WindWavePlane/update");

  UpdateResult result;

  simulation_timer.on_frame_entry(real_dt);
  Stopwatch abort_clock;

  while (simulation_timer.should_proceed(sim_dt)) {
    std::copy(strength_curr.get(), strength_curr.get() + dim * dim, strength_last.get());
    std::fill(strength_curr.get(), strength_curr.get() + dim * dim, Vec2f{});

    for (auto& wave : waves) {
      bool just_elapsed = false;

      switch (wave.type) {
        case WaveType::Hump:
          just_elapsed = hump_wave_update(wave, strength_curr, dim);
          break;
        case WaveType::TravelingCosine:
          just_elapsed = traveling_cosine_wave_update(wave, strength_curr, dim);
          break;
        case WaveType::TransientCosine:
          just_elapsed = transient_cosine_wave_update(wave, strength_curr, dim);
          break;
        default:
          assert(false);
      }

      if (just_elapsed) {
        result.elapsed_waves.push_back(wave.id);
      }
    }

    if (simulation_timer.on_after_simulate_check_abort(sim_dt, abort_clock, sim_dt * 0.5)) {
      GROVE_LOG_WARNING_CAPTURE_META("Wind simulation aborted early.", "WindWavePlane");
      break;
    }
  }

  time_alpha = simulation_timer.get_accumulated_time() / sim_dt;
  return result;
}

WindWave WindWavePlane::create_wave(const Vec2f& dir) {
  WindWave wave{};
  wave.id = next_wave_id++;
  wave.dir = normalize(dir);
  wave.inv_m = wind_direction_to_inverse_matrix(wave.dir);
  return wave;
}

void WindWavePlane::push_wave(const WindWave& wave) {
  waves.push_back(wave);
}

WindWave* WindWavePlane::get_wave(WaveID id) {
  auto it = std::find_if(waves.begin(), waves.end(), [id](const auto& wave) {
    return wave.id == id;
  });
  return it == waves.end() ? nullptr : &(*it);
}

void WindWavePlane::resume(WaveID id) {
  for (auto& wave : waves) {
    if (wave.id == id) {
      wave.elapsed = false;
      return;
    }
  }
  assert(false);
}

//void WindWavePlane::push_hump_wave(const Vec2f& dir) {
//  WindWave wave{};
//  wave.id = next_wave_id++;
//  wave.elapsed = false;
//  wave.type = WaveType::Hump;
//  wave.amplitude = 0.5f;
//  wave.dir = normalize(dir);
//  wave.inv_m = wind_direction_to_inverse_matrix(wave.dir);
//  waves.push_back(wave);
//
//  auto another = wave;
//  another.id = next_wave_id++;
//  another.center = 0.5f;
//  waves.push_back(another);
//}

GROVE_NAMESPACE_END
