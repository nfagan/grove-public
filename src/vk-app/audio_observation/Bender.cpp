#include "Bender.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

using namespace observe;

AudioParameterMonitor::MonitorableNode Bender::make_node(ValueCB&& quant_rep_cb,
                                                         ValueCB&& signal_rep_cb) {
  auto quant_cb_method = AudioParameterMonitor::CallbackMethod::OnUpdate;
  auto signal_cb_method = quant_cb_method;

  auto quant_rep_param = AudioParameterMonitor::make_pending_monitorable_parameter(
    "quantization_representation", std::move(quant_rep_cb), quant_cb_method);

  auto signal_rep_param = AudioParameterMonitor::make_pending_monitorable_parameter(
    "signal_representation", std::move(signal_rep_cb), signal_cb_method);

  AudioParameterMonitor::MonitorableNode node{};
  node.params.push_back(std::move(quant_rep_param));
  node.params.push_back(std::move(signal_rep_param));
  return node;
}

GROVE_NAMESPACE_END