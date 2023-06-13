#pragma once

#include "AudioNodeStorage.hpp"
#include "grove/common/Optional.hpp"

namespace grove::audio {

struct NodeSignalValueSystemStats {
  int num_values;
};

struct NodeSignalValueSystem;

struct ReadSignalValue {
  float value01;
};

NodeSignalValueSystem* get_global_node_signal_value_system();

void update_node_signal_values(
  NodeSignalValueSystem* sys, const AudioNodeStorage& node_storage);

Optional<ReadSignalValue> read_node_signal_value(
  const NodeSignalValueSystem* sys, AudioNodeStorage::NodeID node_id);

void set_node_signal_value01(
  NodeSignalValueSystem* sys, AudioNodeStorage::NodeID node, float value01);

NodeSignalValueSystemStats get_stats(const NodeSignalValueSystem* sys);

}