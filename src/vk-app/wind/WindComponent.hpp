#pragma once

#include "SpatiallyVaryingWind.hpp"
#include "WindDisplacement.hpp"
#include "../particle/WindParticles.hpp"
#include "../render/DynamicSampledImageManager.hpp"

namespace grove {

class WindComponent {
public:
  struct InitInfo {
    vk::DynamicSampledImageManager& image_manager;
    const vk::DynamicSampledImageManager::CreateContext& create_context;
  };

  struct InitResult {
    Optional<vk::DynamicSampledImageManager::Handle> wind_displacement_image;
  };

  struct UpdateInfo {
    vk::DynamicSampledImageManager& image_manager;
    const Vec3f& camera_position;
    double real_dt;
  };
public:
  InitResult initialize(const InitInfo& info);
  void update(const UpdateInfo& info);
  Vec2f approx_displacement_limits() const;
  Vec2f render_axis_strength_limits() const;

public:
  SpatiallyVaryingWind wind;
  WindDisplacement wind_displacement;
  WindParticles wind_particles;
  Optional<vk::DynamicSampledImageManager::Handle> displacement_image_handle;
};

}