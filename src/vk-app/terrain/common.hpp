#pragma once

namespace grove::terrain {

struct GlobalRenderParams {
  float min_shadow{0.5f};
  float global_color_scale{1.0f};
  float frac_global_color_scale{1.0f};
};

}