#pragma once

#include "grass.hpp"
#include "../render/GrassRenderer.hpp"

namespace grove {

namespace weather {
struct Status;
}

class GrassComponent {
  struct InitInfo {
    const Camera& camera;
  };

  struct BeginFrameInfo {
    GrassRenderer& renderer;
    const GrassRenderer::SetDataContext& set_data_context;
  };

  struct UpdateInfo {
    const Camera& camera;
    float follow_distance;
    Vec3f player_position;
    const weather::Status& weather_status;
  };

  struct UpdateResult {
    float min_shadow;
    float global_color_scale;
    float frac_global_color_scale;
  };

public:
  void initialize(const InitInfo& init_info);
  void begin_frame(const BeginFrameInfo& info);
  UpdateResult update(const UpdateInfo& update_info);

private:
  Grass high_lod_grass;
  GrassInitParams high_lod_init_params{};
  bool high_lod_grass_data_updated{};

  Grass low_lod_grass;
  GrassInitParams low_lod_init_params{};
  bool low_lod_grass_data_updated{};

  Stopwatch stopwatch;
};

}