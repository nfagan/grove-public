#pragma once

#include "csm.hpp"

namespace grove {

struct GraphicsGUIUpdateResult;

class ShadowComponent {
public:
  struct InitInfo {
    float sun_shadow_layer_size;
    int num_sun_shadow_cascades;
    int sun_shadow_texture_dim;
    float sun_shadow_projection_sign_y;
  };

public:
  void initialize(const InitInfo& info);
  void update(const Camera& camera, const Vec3f& sun_position);
  const csm::CSMDescriptor& get_sun_csm_descriptor() const {
    return sun_csm_descriptor;
  }
  void on_gui_update(const GraphicsGUIUpdateResult& gui_update_res);

private:
  csm::CSMDescriptor sun_csm_descriptor;
};

}