#include "UIAudioParameterManager.hpp"
#include "grove/common/common.hpp"
#include "grove/common/vector_util.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

inline UIAudioParameterData from_audio_parameter_data(AudioParameterType type,
                                                      const AudioParameterData& data) {
  UIAudioParameterData value{};

  switch (type) {
    case AudioParameterType::Float: {
      value.f = data.f;
      break;
    }
    case AudioParameterType::Int: {
      value.i = data.i;
      break;
    }
    default: {
      assert(false);
    }
  }

  return value;
}

inline void lerp_parameter_value(UIAudioParameter& param, double frac_incr) {
  switch (param.type) {
    case AudioParameterType::Float: {
      auto dist = param.target.f - param.value.f;
      auto incr = frac_incr * dist;
      param.value.f = clamp(float(param.value.f + incr), param.min.f, param.max.f);
      break;
    }
    case AudioParameterType::Int: {
      auto float_v = float(param.value.i);
      auto float_t = float(param.target.i);

      auto dist = float_t - float_v;
      auto incr = frac_incr * dist;
      auto new_value = int(std::round(float_v + incr));

      param.value.i = clamp(new_value, param.min.i, param.max.i);
      break;
    }
    default: {
      assert(false);
    }
  }
}

} //  anon

/*
 * UIAudioParameter
 */

void UIAudioParameter::set_target(const AudioParameterValue& v) {
  assert(type == v.type);
  target = from_audio_parameter_data(type, v.data);
}

void UIAudioParameter::set_value(const AudioParameterValue& v) {
  assert(type == v.type);
  value = from_audio_parameter_data(type, v.data);
}

AudioParameterValue UIAudioParameter::as_audio_parameter_value() const {
  switch (type) {
    case AudioParameterType::Float:
      return make_float_parameter_value(value.f);
    case AudioParameterType::Int:
      return make_int_parameter_value(value.i);
    default:
      assert(false);
      return make_float_parameter_value(0.0f);
  }
}

float UIAudioParameter::float_span() const {
  switch (type) {
    case AudioParameterType::Float: {
      return max.f - min.f;
    }
    case AudioParameterType::Int: {
      return float(max.i) - float(min.i);
    }
    default: {
      assert(false);
      return 0.0f;
    }
  }
}

float UIAudioParameter::fractional_value() const {
  auto span = float_span();
  if (span == 0.0f) {
    return 0.0f;

  } else {
    switch (type) {
      case AudioParameterType::Float: {
        return (value.f - min.f) / span;
      }
      case AudioParameterType::Int: {
        return (float(value.i) - float(min.i)) / span;
      }
      default: {
        assert(false);
        return 0.0f;
      }
    }
  }
}

UIAudioParameter UIAudioParameter::from_descriptor(const AudioParameterDescriptor& descriptor) {
  auto type = descriptor.type;
  auto dflt = from_audio_parameter_data(type, descriptor.dflt);
  auto min = from_audio_parameter_data(type, descriptor.min);
  auto max = from_audio_parameter_data(type, descriptor.max);
  auto target = dflt;
  return UIAudioParameter{type, dflt, target, min, max};
}

double UIAudioParameterManager::tick(double current_stream_time) {
  if (first_update) {
    last_update_stream_time = current_stream_time;
    first_update = false;
  }

  auto delta_t = current_stream_time - last_update_stream_time;
  last_update_stream_time = current_stream_time;
  return delta_t;
}

void UIAudioParameterManager::process_events(const UIParameterChangeList& change_list,
                                             double current_stream_time,
                                             double sample_rate) {
  for (const auto& change_event : change_list.parameter_change_events) {
    assert(change_event.type == AudioEvent::Type::NewAudioParameterValue);
    const auto ids = change_event.data.parameter_change.ids;
    assert(ids.parent != null_audio_parameter_id());

    if (active_ui_parameters.count(ids) > 0) {
      pending_events.push_back(change_event);
    }
  }

  DynamicArray<int64_t, 16> erase_inds;

  for (int64_t i = 0; i < int64_t(pending_events.size()); i++) {
    const auto& change = pending_events[i];
    if (change.time > current_stream_time) {
      continue;
    }

    assert(change.type == AudioEvent::Type::NewAudioParameterValue);
    const auto& new_change = change.data.parameter_change;
    const auto& new_value = new_change.value;
    const auto ids = new_change.ids;

    if (active_ui_parameters.count(ids) > 0) {
      auto& active_param = active_ui_parameters.at(ids);
      double time_to_change = 0.0;

      if (active_param.is_continuous()) {
        //  Continuous parameters lerp towards a target.
        const auto time_err = current_stream_time - change.time;
        time_to_change = new_change.frame_distance_to_target / sample_rate - time_err;

        if (time_to_change <= 0.0) {
          time_to_change = default_immediate_change_distance_seconds();
        }
#if 0
        //  @TODO: This shouldn't be necessary and doesn't seem to be; determine why we put this
        //  snap-to-target behavior here. If it is really necessary to preserve this, we could add
        //  some flags to the parameter struct.
        if (active_param.num_updates_this_frame > 0) {
          //  The parameter already reached its target.
          active_param.value = active_param.target;
        }
#endif
      } else {
        //  Discrete parameters immediately change to a new value, so time_to_change will
        //  always be <= 0.
        assert(new_change.frame_distance_to_target == 0);
        active_param.set_value(new_value);
      }

      active_param.set_target(new_value);
      active_param.time_to_change = float(time_to_change);
      active_param.num_updates_this_frame++;
    }

    erase_inds.push_back(i);
  }

  erase_set(pending_events, erase_inds);
}

void UIAudioParameterManager::update(const UIParameterChangeList& change_list,
                                     double current_stream_time,
                                     double sample_rate) {
  auto dt = tick(current_stream_time);
  reset_parameter_update_counts();
  process_events(change_list, current_stream_time, sample_rate);
  update_parameter_values(dt);
}

void UIAudioParameterManager::reset_parameter_update_counts() {
  for (auto& [_, param] : active_ui_parameters) {
    param.num_updates_this_frame = 0;
  }
}

void UIAudioParameterManager::update_parameter_values(double dt) {
  for (auto& param_it : active_ui_parameters) {
    auto& param = param_it.second;
    auto& time_dist = param.time_to_change;

    if (time_dist > 0.0f) {
      assert(param.is_continuous());
      auto frac_incr = clamp(dt / time_dist, 0.0, 1.0);
      lerp_parameter_value(param, frac_incr);
      time_dist = float(std::max(0.0, time_dist - dt));
    }
  }
}

void UIAudioParameterManager::add_active_ui_parameter(AudioParameterIDs id,
                                                      UIAudioParameter value) {
  assert(id.parent != null_audio_parameter_id());
  value.num_updates_this_frame = 0;
  value.time_to_change = 0.0f;
  active_ui_parameters[id] = value;
}

void UIAudioParameterManager::remove_active_ui_parameter(AudioParameterIDs id) {
  assert(id.parent != null_audio_parameter_id());
  active_ui_parameters.erase(id);
}

Optional<UIAudioParameter> UIAudioParameterManager::read_value(AudioParameterIDs id) const {
  auto it = active_ui_parameters.find(id);
  return it == active_ui_parameters.end() ?
    NullOpt{} : Optional<UIAudioParameter>(it->second);
}

Optional<UIAudioParameter>
UIAudioParameterManager::require_and_read_value(const AudioParameterDescriptor& descriptor) {
  if (auto maybe_val = read_value(descriptor.ids)) {
    return maybe_val;
  }

  auto ui_val = UIAudioParameter::from_descriptor(descriptor);
  add_active_ui_parameter(descriptor.ids, ui_val);

  return read_value(descriptor.ids);
}

GROVE_NAMESPACE_END
