#include "RandomizedEnvelope.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

using namespace observe;

AudioParameterMonitor::MonitorableNode RandomizedEnvelope::make_node(ValueCB&& env_rep_cb) {
  const auto env_cb_method = AudioParameterMonitor::CallbackMethod::OnUpdate;
  auto env_rep_param = AudioParameterMonitor::make_pending_monitorable_parameter(
    "envelope_representation", std::move(env_rep_cb), env_cb_method);

  AudioParameterMonitor::MonitorableNode node{};
  node.params.push_back(std::move(env_rep_param));
  return node;
}

GROVE_NAMESPACE_END
