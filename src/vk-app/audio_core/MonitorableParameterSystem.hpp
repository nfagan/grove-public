#pragma once

#include "UIAudioParameterManager.hpp"
#include "AudioNodeStorage.hpp"

namespace grove::param_system {

struct MonitorableParameterSystemStats {
  int num_parameters;
};

struct MonitorableParameterSystem;

struct ReadMonitorableParameter {
  Optional<AudioParameterDescriptor> desc;
  Optional<UIAudioParameter> value;
  float interpolated_fractional_value;
};

MonitorableParameterSystem* get_global_monitorable_parameter_system();

void update_monitorable_parameter_values(
  MonitorableParameterSystem* sys,
  const AudioNodeStorage& node_storage, UIAudioParameterManager& param_manager, double real_dt);

ReadMonitorableParameter read_monitorable_parameter(
  MonitorableParameterSystem* sys, AudioNodeStorage::NodeID node_id, const char* param,
  float interpolation_power = 0.0f);

MonitorableParameterSystemStats get_stats(const MonitorableParameterSystem* sys);

}