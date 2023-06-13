#include "MonitorableParameterSystem.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace param_system {

struct MonitorableParameter {
  AudioNodeStorage::NodeID node_id{};
  const char* search_name{""};
  Optional<AudioParameterDescriptor> found_desc;
  Optional<UIAudioParameter> latest_value;
  float interpolation_power{};
  float interpolated_fractional_value{};
};

struct MonitorableParameterSystem {
  std::vector<MonitorableParameter> monitorable_parameters;
};

} //  param_sys

namespace {

using namespace param_system;

bool less_by_node_id(const MonitorableParameter& a, AudioNodeStorage::NodeID id) {
  return a.node_id < id;
}

auto find_node_begin(const std::vector<MonitorableParameter>& params, AudioNodeStorage::NodeID id) {
  return std::lower_bound(params.begin(), params.end(), id, less_by_node_id);
}

struct {
  MonitorableParameterSystem sys;
} globals;

} //  anon

MonitorableParameterSystem* param_system::get_global_monitorable_parameter_system() {
  return &globals.sys;
}

void param_system::update_monitorable_parameter_values(
  MonitorableParameterSystem* sys, const AudioNodeStorage& node_storage,
  UIAudioParameterManager& param_manager, double real_dt) {
  //
  auto param_it = sys->monitorable_parameters.begin();
  while (param_it != sys->monitorable_parameters.end()) {
    MonitorableParameter& param = *param_it;

    if (!node_storage.node_exists(param.node_id)) {
      //  Associated node no longer exists, so remove this parameter.
      if (param.found_desc) {
        param_manager.remove_active_ui_parameter(param.found_desc.value().ids);
      }
      param_it = sys->monitorable_parameters.erase(param_it);
      continue;
    }

    if (param.found_desc) {
      param.latest_value = param_manager.read_value(param.found_desc.value().ids);

    } else {
      Temporary<AudioParameterDescriptor, 256> store_view_descs;
      TemporaryViewStack<AudioParameterDescriptor> views = store_view_descs.view_stack();
      node_storage.audio_parameter_descriptors(param_it->node_id, views);

      const AudioParameterDescriptor* dst_desc{};
      for (auto& desc : views) {
        if (std::strcmp(desc.name, param.search_name) == 0) {
          assert(desc.is_monitorable());
          dst_desc = &desc;
          break;
        }
      }

      if (dst_desc) {
        param.found_desc = *dst_desc;
        param.latest_value = param_manager.require_and_read_value(*dst_desc);
        param.interpolated_fractional_value = param.latest_value.unwrap().fractional_value();
      }
    }

    if (param.latest_value && param.interpolation_power > 0.0f) {
      const auto t = 1.0 - std::pow(param.interpolation_power, real_dt);
      const float target = param.latest_value.value().fractional_value();
      const float current = param.interpolated_fractional_value;
      param.interpolated_fractional_value = lerp(t, current, target);
    }

    ++param_it;
  }
}

param_system::ReadMonitorableParameter
param_system::read_monitorable_parameter(MonitorableParameterSystem* sys,
                                         AudioNodeStorage::NodeID node_id, const char* param,
                                         float interpolation_power) {
  auto dst_it = find_node_begin(sys->monitorable_parameters, node_id);
  bool existing_param{};
  while (dst_it != sys->monitorable_parameters.end() && dst_it->node_id == node_id) {
    if (dst_it->search_name == param) {
      existing_param = true;
      break;
    } else {
      ++dst_it;
    }
  }

  if (!existing_param) {
    assert(std::isfinite(interpolation_power) && interpolation_power >= 0.0f);
    MonitorableParameter new_param{};
    new_param.node_id = node_id;
    new_param.search_name = param;
    new_param.interpolation_power = interpolation_power;
    dst_it = sys->monitorable_parameters.insert(dst_it, new_param);
  }

  const MonitorableParameter& dst_param = *dst_it;
  ReadMonitorableParameter result{};
  result.value = dst_param.latest_value;
  result.desc = dst_param.found_desc;
  result.interpolated_fractional_value = dst_param.interpolated_fractional_value;
  return result;
}

MonitorableParameterSystemStats param_system::get_stats(const MonitorableParameterSystem* sys) {
  MonitorableParameterSystemStats result{};
  result.num_parameters = int(sys->monitorable_parameters.size());
  return result;
}

GROVE_NAMESPACE_END
