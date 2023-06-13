#pragma once

#include "sun.hpp"
#include "../render/SkyRenderer.hpp"
#include "SkyGradient.hpp"
#include "SkyProperties.hpp"

namespace grove {

struct SkyGUIUpdateResult;

namespace weather {
struct Status;
}

class SkyComponent {
public:
  struct InitInfo {
    vk::SampledImageManager& image_manager;
    vk::DynamicSampledImageManager& dynamic_image_manager;
    SkyRenderer& renderer;
    const vk::DynamicSampledImageManager::CreateContext& dynamic_image_create_context;
  };

  struct UpdateInfo {
    vk::DynamicSampledImageManager& image_manager;
    const weather::Status& weather_status;
  };

  struct UpdateResult {
    Optional<vk::DynamicSampledImageManager::Handle> sky_image;
  };

public:
  void initialize(const InitInfo& info);
  UpdateResult update(const UpdateInfo& info);
  const Sun& get_sun() const {
    return sun;
  }
  Optional<vk::DynamicSampledImageManager::Handle> get_sky_image() const {
    return sky_image;
  }
  void on_gui_update(const SkyGUIUpdateResult& res);

public:
  Sun sun;

  SkyProperties properties;
  SkyGradient gradient;
  SkyGradient::Params gradient_params_from_user{};
  SkyGradient::Params gradient_params_from_weather{};
  bool weather_controls_sky_gradient{true};
  bool use_sun_angles{};
  bool need_set_default_sun{};
  double sun_position_theta_frac{0.25};
  double sun_position_phi_radians{};

  vk::DynamicSampledImageManager::FutureHandle sky_image_future;
  Optional<vk::DynamicSampledImageManager::Handle> sky_image;
};

}