#pragma once

#include "WindWavePlane.hpp"
#include "WindSpectralInfluence.hpp"
#include "grove/math/Bounds3.hpp"
#include "grove/math/vector.hpp"
#include "grove/common/Optional.hpp"

namespace grove {

class GLRenderContext;
class NewWindSystem;

class SpatiallyVaryingWind {
  friend class DebugSpatiallyVaryingWind;

public:
  SpatiallyVaryingWind();
  ~SpatiallyVaryingWind();

  void update(double real_dt);
  void update_spectrum(const SpectrumAnalyzer::AnalysisFrame& frame);

  Vec2f wind_force(const Vec2f& position_xz) const;
  float wind_force01_no_spectral_influence(const Vec2f& position_xz) const;

  Vec2f to_normalized_position(const Vec2f& p) const;
  const Bounds3f& world_bound() const {
    return wind_bounds;
  }
  Vec4f world_bound_xz() const;
  Vec2f get_dominant_wind_direction() const {
    return dominant_wind_direction;
  }
  void set_dominant_wind_direction(const Vec2f& dir);

  void set_spectral_influence_strength(float v) {
    spectral_influence_strength = clamp(v, 0.0f, 1.0f);
  }
  float get_spectral_influence_strength() const {
    return spectral_influence_strength;
  }

private:
  Vec2f wind_force_normalized_position(const Vec2f& p, float spectral_scale) const;
  void update_wind_direction_change(double real_dt, double sim_dt);

private:
  Bounds3f wind_bounds;
  Vec2f dominant_wind_direction{};
  Vec2f last_dominant_wind_direction{};

  NewWindSystem* wind_system;
  WindWavePlane wave_plane;
  WindSpectralInfluence spectral_influence;

  float theta_current{};
  float theta_target{};
  float spectral_value{};
  float spectral_influence_strength{0.5f};
};

}