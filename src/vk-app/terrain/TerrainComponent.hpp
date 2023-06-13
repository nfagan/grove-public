#pragma once

#include "../render/TerrainRenderer.hpp"
#include "../render/GrassRenderer.hpp"
#include "../terrain/terrain.hpp"
#include "grove/math/Bounds3.hpp"

namespace grove {

namespace weather {
  struct Status;
}

class TerrainComponent {
  struct InitInfo {
    vk::SampledImageManager& image_manager;
    vk::DynamicSampledImageManager& dynamic_image_manager;
    TerrainRenderer& terrain_renderer;
    GrassRenderer& grass_renderer;
    const vk::DynamicSampledImageManager::CreateContext& create_dynamic_image_context;
  };
  struct UpdateInfo {
    const weather::Status& weather_status;
    vk::SampledImageManager& image_manager;
  };
  struct UpdateResult {
    float min_shadow;
    float global_color_scale;
    float frac_global_color_scale;
    Optional<vk::SampledImageManager::Handle> new_material_image_handle;
  };

public:
  void initialize(const InitInfo& info);
  const Terrain& get_terrain() const {
    return terrain;
  }
  UpdateResult update(const UpdateInfo& info);
  Bounds3f world_aabb() const;
  void set_new_material_image_file_path(const std::string& p, bool prepend_asset_dir);

private:
  Terrain terrain;
  Optional<std::string> new_material_image_file_path;
  vk::SampledImageManager::Handle new_material_image_handle{};
};

}