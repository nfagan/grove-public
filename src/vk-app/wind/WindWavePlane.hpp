#pragma once

#include "grove/math/vector.hpp"
#include "grove/math/matrix.hpp"
#include "grove/common/SimulationTimer.hpp"
#include "grove/common/DynamicArray.hpp"
#include <vector>
#include <memory>

namespace grove {

class WindWavePlane {
public:
  using Strength = std::unique_ptr<Vec2f[]>;
  using WaveID = uint64_t;

  enum class WaveType {
    Hump,
    TravelingCosine,
    TransientCosine
  };

  struct WindWave {
    WaveID id{};
    WaveType type{WaveType::Hump};
    float center{};
    float width{0.2f};
    float amplitude{1.0f};
    float incr{0.002f};
    Mat2f inv_m{1.0f};
    Vec2f dir{};
    bool elapsed{};
  };

  struct UpdateResult {
    DynamicArray<WaveID, 4> elapsed_waves;
  };

public:
  UpdateResult update(double real_dt, double sim_dt);
  WindWave create_wave(const Vec2f& dir);

  void push_wave(const WindWave& wave);
  void resume(WaveID id);
  WindWave* get_wave(WaveID id);

  Vec2f evaluate_wave(const Vec2f& frac_p) const;
  void set_dominant_wind_direction(const Vec2f& dir);

private:
  std::vector<WindWave> waves;

  int dim{64};
  Strength strength_last{std::make_unique<Vec2f[]>(dim * dim)};
  Strength strength_curr{std::make_unique<Vec2f[]>(dim * dim)};

  SimulationTimer simulation_timer;
  double time_alpha{};

  WaveID next_wave_id{1};
};

}