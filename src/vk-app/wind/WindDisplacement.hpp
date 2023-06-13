#pragma once

#include "grove/math/Bounds3.hpp"
#include "grove/math/vector.hpp"
#include "grove/common/SimulationTimer.hpp"
#include <memory>

#define GROVE_DAMPED_SPRING_TIP_DISPLACEMENT (0)

namespace grove {

class SpatiallyVaryingWind;

class WindDisplacement {
#if GROVE_DAMPED_SPRING_TIP_DISPLACEMENT
  struct SamplePoint {
    Vec2f position{};
    Vec2f velocity{};
    float k{256.0f};
    float w0{10.0f};
    float zeta{0.8f};
  };
#else
  struct SamplePoint {
    Vec2f position{};
    Vec2f velocity{};
    float k{256.0f};
    float m{1.0f};
  };
#endif

public:
  using Samples = std::unique_ptr<SamplePoint[]>;
  using Displacement = std::unique_ptr<Vec2f[]>;

public:
  WindDisplacement();

  void initialize(const SpatiallyVaryingWind& wind);
  void update(const SpatiallyVaryingWind& wind, double real_dt);

  Vec2f evaluate(const Vec2f& frac_p) const;

  const Displacement& read_displacement() const {
    return displacement;
  }

  int texture_dim() const {
    return dim;
  }

  size_t displacement_size_bytes() const {
    return sizeof(Vec2f) * texture_dim() * texture_dim();
  }

  float approx_gust_magnitude() const;
  float approx_idle_magnitude() const;

private:
  int dim;
  Displacement displacement;
  Samples samples_prev;
  Samples samples_curr;
  SimulationTimer simulation_timer;
};

}