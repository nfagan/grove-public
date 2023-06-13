#pragma once

#include "AudioNodeStorage.hpp"
#include "grove/audio/AudioParameterSystem.hpp"

namespace grove {
struct SimpleSetParameterSystem;
}

namespace grove::param_system {

SimpleSetParameterSystem* get_global_simple_set_parameter_system();

void ui_initialize(
  SimpleSetParameterSystem* sys, const AudioNodeStorage* node_storage,
  AudioParameterSystem* param_sys);
void ui_evaluate_deleted_nodes(
  SimpleSetParameterSystem* sys, const ArrayView<AudioNodeStorage::NodeID>& del);

bool ui_set_float_value_from_fraction(
  SimpleSetParameterSystem* sys, AudioNodeStorage::NodeID node, const char* pname, float v01);
bool ui_set_int_value(
  SimpleSetParameterSystem* sys, AudioNodeStorage::NodeID node, const char* pname, int val);

}