#include "NodeSignalValueSystem.hpp"
#include "grove/common/common.hpp"
#include <unordered_map>

GROVE_NAMESPACE_BEGIN

namespace audio {

struct NodeSignalValue {
  float value01;
};

struct NodeSignalValueSystem {
  std::unordered_map<AudioNodeStorage::NodeID, NodeSignalValue> values;
};

} //  audio

namespace {

struct {
  audio::NodeSignalValueSystem system;
} globals;

} //  anon

audio::NodeSignalValueSystem* audio::get_global_node_signal_value_system() {
  return &globals.system;
}

void audio::update_node_signal_values(
  NodeSignalValueSystem* sys, const AudioNodeStorage& node_storage) {
  //
  auto it = sys->values.begin();
  while (it != sys->values.end()) {
    if (!node_storage.node_exists(it->first)) {
      it = sys->values.erase(it);
    } else {
      ++it;
    }
  }
}

Optional<audio::ReadSignalValue> audio::read_node_signal_value(
  const NodeSignalValueSystem* sys, AudioNodeStorage::NodeID node_id) {
  //
  auto it = sys->values.find(node_id);
  if (it == sys->values.end()) {
    return NullOpt{};
  } else {
    audio::ReadSignalValue res{};
    res.value01 = it->second.value01;
    return Optional<audio::ReadSignalValue>(res);
  }
}

void audio::set_node_signal_value01(
  NodeSignalValueSystem* sys, AudioNodeStorage::NodeID node, float value01) {
  //
  assert(node != 0);
  assert(value01 >= 0.0f && value01 <= 1.0f);
  NodeSignalValue val{};
  val.value01 = value01;
  sys->values[node] = val;
}

audio::NodeSignalValueSystemStats audio::get_stats(const NodeSignalValueSystem* sys) {
  audio::NodeSignalValueSystemStats result{};
  result.num_values = int(sys->values.size());
  return result;
}

GROVE_NAMESPACE_END
