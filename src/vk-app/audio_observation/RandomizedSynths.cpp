#include "RandomizedSynths.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

using namespace observe;

AudioParameterMonitor::MonitorableNode RandomizedSynths::make_node(ValueCB env_cb,
                                                                   ValueCB note_cb) {
  auto env_param = AudioParameterMonitor::make_pending_monitorable_parameter(
    "envelope_representation", std::move(env_cb), AudioParameterMonitor::CallbackMethod::OnChange);
  auto note_param = AudioParameterMonitor::make_pending_monitorable_parameter(
    "new_note_number_representation", std::move(note_cb), AudioParameterMonitor::CallbackMethod::OnUpdate);

  AudioParameterMonitor::MonitorableNode monitorable_node{};
  monitorable_node.params.push_back(std::move(env_param));
  monitorable_node.params.push_back(std::move(note_param));
  return monitorable_node;
}

GROVE_NAMESPACE_END
