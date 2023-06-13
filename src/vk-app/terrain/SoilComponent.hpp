#pragma once

#include "Soil.hpp"
#include "../render/DebugImageRenderer.hpp"

namespace grove {

struct SoilGUIUpdateResult;

class SoilComponent {
  friend class SoilGUI;
public:
  struct Params {
    bool enabled{};
    bool initialized{};
    bool draw_debug_image{};
    bool overlay_player_position{true};
    float overlay_radius{8.0f};
  };

  struct InitInfo {
    vk::DynamicSampledImageManager& image_manager;
    const vk::DynamicSampledImageManager::CreateContext& image_create_context;
  };
  struct UpdateInfo {
    vk::DynamicSampledImageManager& image_manager;
    const Vec2f& debug_position_xz;
  };
  struct UpdateResult {
    Optional<vk::DynamicSampledImageManager::Handle> show_debug_image;
    DebugImageRenderer::DrawableParams debug_image_params;
  };

public:
  void initialize(const InitInfo& info);
  [[nodiscard]] UpdateResult update(const UpdateInfo& info);
  void on_gui_update(const SoilGUIUpdateResult& res);
  const Soil* get_soil() const {
    return &soil;
  }
  Soil* get_soil() {
    return &soil;
  }

private:
  Soil soil;
  Params params;
  Optional<vk::DynamicSampledImageManager::Handle> debug_image_handle;
};

}