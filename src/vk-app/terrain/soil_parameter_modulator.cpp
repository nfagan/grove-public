#include "soil_parameter_modulator.hpp"
#include "../imgui/SoilGUI.hpp"
#include "grove/audio/AudioParameterSystem.hpp"
#include "grove/common/common.hpp"
#include "grove/math/util.hpp"

GROVE_NAMESPACE_BEGIN

using namespace soil;

void soil::update_parameter_modulator(ParameterModulator& modulator,
                                      const ParameterModulatorUpdateContext& context) {
  auto& write_access = *param_system::ui_get_write_access(context.parameter_system);
  auto& selected = context.selected_node;
  if (!modulator.parameter_writer_id) {
    modulator.parameter_writer_id = AudioParameterWriteAccess::create_writer();
  }

  bool need_release;
  if (!modulator.enabled) {
    need_release = !modulator.targets.empty();
  } else if (selected) {
    need_release = !modulator.lock_targets &&
                   !modulator.targets.empty() &&
                   selected.value() != modulator.target_node;
  } else {
    need_release = !modulator.lock_targets && !modulator.targets.empty();
  }

  if (need_release) {
    for (auto& target : modulator.targets) {
      write_access.release(modulator.parameter_writer_id.value(), target);
    }
    modulator.targets.clear();
    modulator.target_node = {};
  }

  if (modulator.targets.empty() && modulator.enabled && selected) {
    Temporary<AudioParameterDescriptor, 16> tmp_desc;
    auto tmp_view_desc = tmp_desc.view_stack();
    auto params = context.node_storage.audio_parameter_descriptors(selected.value(), tmp_view_desc);
    for (auto& param : params) {
      if (param.is_editable() &&
          param.is_float() &&
          write_access.request(modulator.parameter_writer_id.value(), param)) {
        modulator.targets.push_back(param);
        modulator.target_node = selected.value();
      }
    }
  }

  for (auto& target : modulator.targets) {
    auto len = clamp(context.soil_quality.length() / std::sqrt(3.0f), 0.0f, 1.0f);
    auto val = make_interpolated_parameter_value_from_descriptor(target, len);
    param_system::ui_set_value(
      context.parameter_system, modulator.parameter_writer_id.value(), target.ids, val);
  }
}

void soil::on_gui_update(ParameterModulator& modulator, const SoilGUIUpdateResult& res) {
  if (res.parameter_capture_enabled) {
    modulator.enabled = res.parameter_capture_enabled.value();
  }
  if (res.lock_parameter_targets) {
    modulator.lock_targets = res.lock_parameter_targets.value();
  }
}

GROVE_NAMESPACE_END
