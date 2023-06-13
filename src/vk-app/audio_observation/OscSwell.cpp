#include "OscSwell.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

using namespace observe;

AudioParameterMonitor::MonitorableNode observe::OscSwell::make_node(ValueCB&& signal_rep_cb) {
  const auto signal_cb_method = AudioParameterMonitor::CallbackMethod::OnUpdate;
  auto signal_rep_param = AudioParameterMonitor::make_pending_monitorable_parameter(
    "signal_representation", std::move(signal_rep_cb), signal_cb_method);

  AudioParameterMonitor::MonitorableNode node{};
  node.params.push_back(std::move(signal_rep_param));
  return node;
}

GROVE_NAMESPACE_END
