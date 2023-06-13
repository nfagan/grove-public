#include "TriggeredOsc.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

using namespace observe;

AudioParameterMonitor::MonitorableNode
observe::TriggeredOsc::make_node(ValueCB&& signal_rep, ValueCB&& note_num) {
  const auto cb_method = AudioParameterMonitor::CallbackMethod::OnUpdate;

  auto signal_rep_param = AudioParameterMonitor::make_pending_monitorable_parameter(
    "signal_representation", std::move(signal_rep), cb_method);
  auto note_num_param = AudioParameterMonitor::make_pending_monitorable_parameter(
    "monitor_note_number", std::move(note_num), cb_method);

  AudioParameterMonitor::MonitorableNode node{};
  node.params.push_back(std::move(signal_rep_param));
  node.params.push_back(std::move(note_num_param));
  return node;
}

GROVE_NAMESPACE_END
