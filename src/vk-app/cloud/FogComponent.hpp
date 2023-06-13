#pragma once

#include "worley.hpp"
#include "transient_mist.hpp"
#include "../render/CloudRenderer.hpp"
#include "../transform/transform_system.hpp"
#include <memory>
#include <future>

namespace grove {

struct FogGUIUpdateResult;
class Terrain;
class SpatiallyVaryingWind;

namespace weather {
struct Status;
}

class FogComponent {
public:
  struct InitInfo {
    transform::TransformSystem& transform_system;
    const vk::DynamicSampledImageManager::CreateContext& image_context;
    vk::DynamicSampledImageManager& image_manager;
  };
  struct InitResult {
    std::vector<transform::TransformInstance*> add_transform_editor;
  };

  struct UpdateInfo {
    const CloudRenderer::AddResourceContext& renderer_context;
    CloudRenderer& cloud_renderer;
    const vk::DynamicSampledImageManager::CreateContext& image_context;
    vk::DynamicSampledImageManager& image_manager;
    double real_dt;
    const Vec2f& wind_direction;
    float wind_force;
    const weather::Status& weather_status;
    const Camera& camera;
    const Terrain& terrain;
    const SpatiallyVaryingWind& wind;
  };

  struct WorleyNoiseFutureData {
    std::unique_ptr<uint8_t[]> data;
    image::Descriptor desc;
  };

  struct TransientMistDrawable {
    CloudRenderer::BillboardDrawableHandle drawable;
    CloudRenderer::BillboardDrawableParams drawable_params;
    Vec3f uvw_scale{1.0f};
  };

public:
  InitResult initialize(const InitInfo& info);
  void update(const UpdateInfo& info);
  void on_gui_update(const FogGUIUpdateResult& res);

private:
  void set_common_fog_config(const UpdateInfo& info);

public:
  WorleyNoiseFutureData fog_data;
  int num_fog_image_components{1};
  bool awaiting_noise_result{};
  bool recompute_noise{};
  bool make_fog{};
  bool wind_influence_enabled{};
  float wind_influence_scale{0.001f};
  bool manual_density_scale{true};
  float weather_driven_density_scale{1.0f};
  Vec3f fog_color{1.0f};
  worley::Parameters worley_noise_params{};
  std::future<WorleyNoiseFutureData> worley_noise_future;

  Optional<CloudRenderer::VolumeDrawableHandle> debug_fog_drawable;
  CloudRenderer::VolumeDrawableParams debug_drawable_params{};
  Optional<vk::DynamicSampledImageManager::Handle> fog_image;
  vk::DynamicSampledImageManager::FutureHandle fog_image_future;
  transform::TransformInstance* debug_transform;

  Optional<CloudRenderer::BillboardDrawableHandle> debug_billboard_drawable;
  CloudRenderer::BillboardDrawableParams debug_billboard_params{};
  transform::TransformInstance* billboard_transform;

  std::array<TransientMistDrawable, 16> transient_mist_drawables{};
  std::array<fog::TransientMistElement, 16> transient_mist_elements{};
  int num_transient_mists{};
  bool initialized_transient_mists{};
};

}