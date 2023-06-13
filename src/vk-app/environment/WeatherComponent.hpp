#pragma once

#include "../weather/WeatherSystem.hpp"
#include "RainParticles.hpp"
#include "../render/RainParticleRenderer.hpp"
#include "grove/common/ArrayView.hpp"

namespace grove {

struct WeatherGUIUpdateResult;

class WeatherComponent {
  friend class WeatherGUI;

public:
  struct InitInfo {
    const RainParticleRenderer::AddResourceContext& context;
    RainParticleRenderer& rain_particle_renderer;
  };
  struct SoilDeposit {
    Vec2f position;
    float radius;
    Vec3f amount;
  };
  struct UpdateResult {
    weather::Status weather_status;
    ArrayView<SoilDeposit> soil_deposits;
  };
  struct UpdateInfo {
    RainParticleRenderer& renderer;
    const Camera& camera;
    const SpatiallyVaryingWind& wind;
    const Vec3f& player_position;
    double real_dt;
  };
  struct Params {
    double rain_particle_dt_scale{1.25f};
    float rain_particle_alpha_scale{0.5f};
    float manual_rain_particle_alpha_scale{0.5f};
    Vec2f rain_particle_scale{0.025f, 0.5f};
    bool override_weather_control{};
  };
public:
  void initialize(const InitInfo& info);
  UpdateResult update(const UpdateInfo& info);
  void on_gui_update(const WeatherGUIUpdateResult& update_res);

private:
  void update_rain_particle_renderer(const UpdateInfo& info, const weather::Status& status);

private:
  WeatherSystem weather_system;
  RainParticles rain_particles;
  std::vector<SoilDeposit> reserve_soil_deposits;

  Optional<RainParticleGroupID> debug_particle_group_id;
  Optional<RainParticleRenderer::DrawableHandle> rain_particle_drawable;
  Params params{};
};

}