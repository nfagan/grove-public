#include "RandomizedInstrumentNodes.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

using namespace observe;

AudioParameterMonitor::MonitorableNode
RandomizedInstrumentNodes::make_node(OnNewParameterValue on_signal_change,
                                     OnNewParameterValue on_note_change) {
  auto wrap_signal_change = [change = std::move(on_signal_change)](const AudioParameterDescriptor&,
                                                                   const UIAudioParameter& param) {
    if (change) {
      change(param.fractional_value());
    }
  };
  auto wrap_note_change = [change = std::move(on_note_change)](const AudioParameterDescriptor&,
                                                               const UIAudioParameter& param) {
    if (change) {
      change(param.fractional_value());
    }
  };

  auto signal_cb_method = AudioParameterMonitor::CallbackMethod::OnChange;
  auto signal_param = AudioParameterMonitor::make_pending_monitorable_parameter(
    "signal_representation", std::move(wrap_signal_change), signal_cb_method);

  auto note_cb_method = AudioParameterMonitor::CallbackMethod::OnChange;
  auto note_param = AudioParameterMonitor::make_pending_monitorable_parameter(
    "note_number_representation", std::move(wrap_note_change), note_cb_method);

  AudioParameterMonitor::MonitorableNode result;
  result.params.push_back(std::move(signal_param));
  result.params.push_back(std::move(note_param));
  return result;
}

GROVE_NAMESPACE_END