#include "BoundsComponent.hpp"
#include "../imgui/SystemsGUI.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

void BoundsComponent::initialize(const InitInfo& info) {
  create_accel_instance_params.initial_span_size = 256.0f;
  create_accel_instance_params.max_span_size_split = 8.0f;  //  @TODO: Experiment with this.
  default_accel = bounds::create_instance(info.bounds_system, create_accel_instance_params);
}

void BoundsComponent::on_gui_update(const SystemsGUIUpdateResult& gui_res) {
  if (gui_res.default_build_params) {
    create_accel_instance_params = gui_res.default_build_params.value();
  }
}

GROVE_NAMESPACE_END