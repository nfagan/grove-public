#include "AudioParameterMonitor.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"

GROVE_NAMESPACE_BEGIN

using namespace observe;

namespace {

[[maybe_unused]] constexpr const char* logging_id() {
  return "AudioParameterMonitor";
}

bool should_callback(const UIAudioParameter& param,
                     const AudioParameterValue& last_value,
                     AudioParameterMonitor::CallbackMethod method) {
  switch (method) {
    case AudioParameterMonitor::CallbackMethod::Always:
      return true;
    case AudioParameterMonitor::CallbackMethod::OnUpdate:
      return param.num_updates_this_frame > 0;
    case AudioParameterMonitor::CallbackMethod::OnChange:
      return param.as_audio_parameter_value() != last_value;
    default:
      assert(false);
      return false;
  }
}

void update_new_parameter_values(AudioNodeStorage::NodeID node_id,
                                 AudioParameterMonitor::MonitorableNode& node,
                                 const AudioNodeStorage& node_storage,
                                 UIAudioParameterManager& parameter_manager) {
#ifdef GROVE_DEBUG
  bool instance_created = node_storage.is_instance_created(node_id);
#endif
  Temporary<AudioParameterDescriptor, 256> tmp_desc;
  auto tmp_view_desc = tmp_desc.view_stack();
  auto param_descriptors = node_storage.audio_parameter_descriptors(node_id, tmp_view_desc);

  for (auto& param : node.params) {
    auto monitoring_param = filter_audio_parameter_descriptors(
      param_descriptors, [&param](auto&& desc) {
        return desc.is_monitorable() && desc.matches_name(param.name);
      });

    if (monitoring_param.size() == 1) {
      auto& descriptor = *monitoring_param[0];
      param.ids = descriptor.ids;

      if (auto maybe_value = parameter_manager.require_and_read_value(descriptor)) {
        auto& ui_param = maybe_value.value();
        auto param_val = ui_param.as_audio_parameter_value();
        if (param.callback && should_callback(ui_param, param.last_value, param.callback_method)) {
          param.callback(descriptor, ui_param);
        }
        param.last_value = param_val;
      }
    } else {
#ifdef GROVE_DEBUG
      if (instance_created) {
        std::string msg{"No such parameter: "};
        msg += param.name;
        GROVE_LOG_ERROR_CAPTURE_META(msg.c_str(), logging_id());
      }
#endif
    }
  }
}

} //  anon

void AudioParameterMonitor::add_node(AudioNodeStorage::NodeID id, MonitorableNode&& node) {
  assert(nodes.count(id) == 0);
  nodes[id] = std::move(node);
}

void AudioParameterMonitor::remove_node(AudioNodeStorage::NodeID node_id,
                                        UIAudioParameterManager& parameter_manager) {
  if (auto it = nodes.find(node_id); it != nodes.end()) {
    auto& node = it->second;
    for (auto& param : node.params) {
      if (param.ids != null_audio_parameter_ids()) {
        parameter_manager.remove_active_ui_parameter(param.ids);
      }
    }
    nodes.erase(it);

  } else {
    assert(false);
  }
}

void AudioParameterMonitor::update(UIAudioParameterManager& parameter_manager,
                                   const AudioNodeStorage& node_storage) {
  for (auto& [id, node] : nodes) {
    update_new_parameter_values(id, node, node_storage, parameter_manager);
  }
}

AudioParameterMonitor::MonitorableParameter
AudioParameterMonitor::make_pending_monitorable_parameter(const char* name,
                                                          ValueCallback&& callback,
                                                          CallbackMethod callback_method) {
  MonitorableParameter result{};
  result.name = name;
  result.callback = std::move(callback);
  result.callback_method = callback_method;
  return result;
}

GROVE_NAMESPACE_END
