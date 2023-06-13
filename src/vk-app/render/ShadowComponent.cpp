#include "ShadowComponent.hpp"
#include "../imgui/GraphicsGUI.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

void ShadowComponent::initialize(const InitInfo& info) {
  std::vector<float> layer_sizes(info.num_sun_shadow_cascades);
  std::fill(layer_sizes.begin(), layer_sizes.end(), info.sun_shadow_layer_size);
  sun_csm_descriptor = csm::make_csm_descriptor(
    info.num_sun_shadow_cascades,
    info.sun_shadow_texture_dim,
    layer_sizes.data(),
    info.sun_shadow_projection_sign_y);
}

void ShadowComponent::update(const Camera& camera, const Vec3f& sun_position) {
  csm::update_csm_descriptor(sun_csm_descriptor, camera, sun_position);
}

void ShadowComponent::on_gui_update(const GraphicsGUIUpdateResult& gui_update_res) {
  auto& shadow_params = gui_update_res.shadow_component_params;
  if (shadow_params.projection_sign_y) {
    sun_csm_descriptor.sign_y = shadow_params.projection_sign_y.value();
  }
}

GROVE_NAMESPACE_END
