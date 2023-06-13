#include "debug_audio_parameter_events.hpp"
#include "grove/audio/audio_node.hpp"
#include "grove/audio/oscillator.hpp"
#include "grove/audio/AudioEventSystem.hpp"
#include "grove/audio/AudioParameterSystem.hpp"
#include "AudioNodeStorage.hpp"
#include "UIAudioParameterManager.hpp"
#include "../terrain/terrain.hpp"
#include "../render/debug_draw.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/math/random.hpp"
#include <imgui/imgui.h>

#define DEBUG_PARAMS (1)

GROVE_NAMESPACE_BEGIN

namespace {

/*
 * Params
 */

class DebugParamsProcessor : public AudioProcessorNode {
public:
  struct Parameters {
    AudioParameter<float, StaticLimits01<float>> gain{1.0f};
    AudioParameter<int, StaticIntLimits<0, 3>> freq{0};
  };

public:
  DebugParamsProcessor(AudioParameterID node_id,
                       const AudioParameterSystem* param_sys) :
    node_id{node_id},
    parameter_system{param_sys} {

    osc.set_frequency(note_to_frequency(pcs[0], 3));

    for (int i = 0; i < 2; i++) {
      output_ports.push_back(OutputAudioPort{BufferDataType::Float, this, i});
    }
  }
  ~DebugParamsProcessor() override = default;
  GROVE_DECLARE_AUDIO_NODE_INTERFACE()

  static void static_parameter_descriptors(uint32_t node_id,
                                           TemporaryViewStack<AudioParameterDescriptor>& out);

private:
  AudioParameterID node_id;
  OutputAudioPorts output_ports;

  const AudioParameterSystem* parameter_system;
  Parameters params;
  PitchClass pcs[4]{PitchClass::C, PitchClass::D, PitchClass::E, PitchClass::G};

  osc::Sin osc{default_sample_rate()};
};

void DebugParamsProcessor::static_parameter_descriptors(uint32_t node,
                                                        TemporaryViewStack<AudioParameterDescriptor>& mem) {
  const int num_desc = 2;
  auto* dst = mem.push(num_desc);
  int i{};
  uint32_t p{};
  Parameters params;
  dst[i++] = params.gain.make_descriptor(node, p++, params.gain.value, "gain");
  dst[i++] = params.freq.make_descriptor(node, p++, params.freq.value, "freq");
}

void DebugParamsProcessor::process(const AudioProcessData&, const AudioProcessData& out,
                                   AudioEvents*, const AudioRenderInfo& info) {
  GROVE_ASSERT_MATCHES_PORT_LAYOUT(out, output_ports);

  osc.set_sample_rate(info.sample_rate);

  auto& changes = param_system::render_read_changes(parameter_system);
  auto self_changes = changes.view_by_parent(node_id);
  auto gain_changes = self_changes.view_by_parameter(0);
  auto freq_changes = self_changes.view_by_parameter(1);

  int gain_ind{};
  int freq_ind{};

  float latest_val{};
  for (int i = 0; i < info.num_frames; i++) {
    maybe_apply_change(gain_changes, gain_ind, params.gain, i);
    maybe_apply_change(freq_changes, freq_ind, params.freq, i);
    osc.set_frequency(note_to_frequency(pcs[params.freq.evaluate()], 3));

    float amp_val = params.gain.evaluate();
    auto val = osc.tick() * amp_val;
    latest_val = amp_val;
    for (int j = 0; j < 2; j++) {
      out.descriptors[j].write(out.buffer.data, i, &val);
    }
  }
}

InputAudioPorts DebugParamsProcessor::inputs() const {
  return {};
}

OutputAudioPorts DebugParamsProcessor::outputs() const {
  return output_ports;
}

/*
 * Events
 */

class DebugEventsProcessor : public AudioProcessorNode {
public:
  struct Parameters {
    AudioParameter<float, StaticLimits01<float>> amp_mod_frequency{0.0f};
    AudioParameter<float, StaticLimits01<float>> signal_repr{0.0f};
  };

public:
  DebugEventsProcessor(AudioParameterID node_id, const AudioParameterSystem* param_sys) :
  node_id{node_id},
  parameter_system{param_sys} {
    for (int i = 0; i < 2; i++) {
      output_ports.push_back(OutputAudioPort{BufferDataType::Float, this, i});
    }
  }
  ~DebugEventsProcessor() override = default;
  GROVE_DECLARE_AUDIO_NODE_INTERFACE()

  static void static_parameter_descriptors(uint32_t node_id,
                                           TemporaryViewStack<AudioParameterDescriptor>& out);

private:
  AudioParameterID node_id;
  OutputAudioPorts output_ports;

  Parameters params;
  const AudioParameterSystem* parameter_system;

  osc::Sin osc{default_sample_rate()};
  osc::Sin amp_mod{default_sample_rate()};
};

void DebugEventsProcessor::static_parameter_descriptors(uint32_t node_id,
                                                        TemporaryViewStack<AudioParameterDescriptor>& mem) {
  const auto monitor_flags = AudioParameterDescriptor::Flags::marked_monitorable_non_editable();
  const int num_desc = 2;
  auto* dst = mem.push(num_desc);
  int i{};
  uint32_t p{};
  Parameters params;
  dst[i++] = params.amp_mod_frequency.make_descriptor(
    node_id, p++, params.amp_mod_frequency.value, "amp_mod_frequency");
  dst[i++] = params.signal_repr.make_descriptor(
    node_id, p++, params.signal_repr.value, "signal_repr", monitor_flags);
}

void DebugEventsProcessor::process(const AudioProcessData&, const AudioProcessData& out,
                                   AudioEvents*, const AudioRenderInfo& info) {
  GROVE_ASSERT_MATCHES_PORT_LAYOUT(out, output_ports);

  osc.set_sample_rate(info.sample_rate);
  osc.set_frequency(frequency_a4());
  amp_mod.set_sample_rate(info.sample_rate);

  auto& changes = param_system::render_read_changes(parameter_system);
  auto self_changes = changes.view_by_parent(node_id);
  auto amp_mod_freq_changes = self_changes.view_by_parameter(0);
  int amp_mod_freq_index{};

  float latest_val{};
  for (int i = 0; i < info.num_frames; i++) {
    maybe_apply_change(amp_mod_freq_changes, amp_mod_freq_index, params.amp_mod_frequency, i);
    auto mod_freq = params.amp_mod_frequency.evaluate() * 1.0f + 1.0f;
    amp_mod.set_frequency(mod_freq);

    float amp_val = (amp_mod.tick() * 0.5f + 0.5f);
    auto val = osc.tick() * amp_val;
    latest_val = amp_val;
    for (int j = 0; j < 2; j++) {
      out.descriptors[j].write(out.buffer.data, i, &val);
    }
  }

  if (info.num_frames > 0) {
    auto evt = make_monitorable_parameter_audio_event(
      {node_id, 1}, make_float_parameter_value(latest_val), info.num_frames - 1, 0);
    evt.frame = info.num_frames - 1;
    auto evt_stream = audio_event_system::default_event_stream();
    (void) audio_event_system::render_push_event(evt_stream, evt);

#if 1
    const int num_push = int(urand() * 128.0);
    for (int i = 0; i < num_push; i++) {
      AudioEvent dummy_evt{};
      dummy_evt.frame = uint64_t(urand() * info.num_frames);
      (void) audio_event_system::render_push_event(evt_stream, dummy_evt);
    }
#endif
  }
}

InputAudioPorts DebugEventsProcessor::inputs() const {
  return {};
}

OutputAudioPorts DebugEventsProcessor::outputs() const {
  return output_ports;
}

struct DebugProcessorNodeInfo {
  AudioNodeStorage::NodeID node_id;
  Vec3f position;
  bool added_break_points{};
  AudioParameterWriterID param_writer{};
};

struct GlobalData {
  bool initialized{};
  BreakPointSetHandle bp_set{};
  DynamicArray<DebugProcessorNodeInfo, 4> node_info;
} globals;

} //  anon

SimpleAudioNodePlacement::CreateNodeResult
debug::initialize_debug_audio_parameter_events(const DebugAudioParameterEventsContext& ctx) {
#if 1
  (void) ctx;
  return {};
#else
  assert(!globals.initialized);

#if DEBUG_PARAMS
  auto node_ctor = [param_sys = ctx.param_sys](AudioNodeStorage::NodeID id) {
    return new DebugParamsProcessor(id, param_sys);
  };
#else
  auto node_ctor = [param_sys = ctx.param_sys](AudioNodeStorage::NodeID id) {
    return new DebugEventsProcessor(id, param_sys);
  };
#endif

#if DEBUG_PARAMS
  auto gather_descs = &DebugParamsProcessor::static_parameter_descriptors;
#else
  auto gather_descs = &DebugEventsProcessor::static_parameter_descriptors;
#endif

  globals.bp_set = param_system::ui_create_break_point_set(
    ctx.param_sys, ScoreRegion{{}, ScoreCursor{4, 0.0}});

  SimpleAudioNodePlacement::CreateNodeResult result{};
  const int num_nodes = 16;
  for (int i = 0; i < num_nodes; i++) {
    DebugProcessorNodeInfo node_info{};
    node_info.node_id = ctx.node_storage.create_node(
      node_ctor, make_port_descriptors_from_audio_node_ctor(node_ctor), gather_descs);
    node_info.param_writer = AudioParameterWriteAccess::create_writer();

    Vec3f p{};
    p.y = 2.0f;
    p.z = 32.0f;
    p.x = 32.0f + float(i) * 4.0f;
    p.y += ctx.terrain.height_nearest_position_xz(p);
    node_info.position = p;

    auto simple_node_res = ctx.node_placement.create_node(
      node_info.node_id,
      ctx.node_storage.get_port_info_for_node(node_info.node_id).unwrap(),
      p, 2.0f);
    for (auto& entry : simple_node_res) {
      result.push_back(entry);
    }

    globals.node_info.push_back(std::move(node_info));
  }

  globals.initialized = true;
  return result;
#endif
}

void debug::update_debug_audio_parameter_events(const DebugAudioParameterEventsContext& ctx) {
  if (!globals.initialized) {
    return;
  }

#if 1
  for (auto& node : globals.node_info) {
    if (!ctx.node_storage.is_instance_created(node.node_id)) {
      continue;
    }

    Temporary<AudioParameterDescriptor, 256> tmp_params;
    auto param_view = tmp_params.view_stack();
    auto params = ctx.node_storage.audio_parameter_descriptors(node.node_id, param_view);
    auto target_param = filter_audio_parameter_descriptors(params, [](auto& p) {
#if DEBUG_PARAMS
      return p.matches_name("gain");
#else
      return p.matches_name("signal_repr");
#endif
    });

    Vec3f color{1.0f};
#if DEBUG_PARAMS
    auto freq_param = filter_audio_parameter_descriptors(params, [](auto& p) {
      return p.matches_name("freq");
    });
    if (freq_param.size() == 1) {
      if (auto val = ctx.ui_parameter_manager.require_and_read_value(*freq_param[0])) {
        Vec3f colors[4] = {
          Vec3f{1.0f, 0.0f, 0.0f}, Vec3f{0.0f, 1.0f, 0.0f},
          Vec3f{0.0f, 0.0f, 1.0f}, Vec3f{1.0f, 1.0f, 0.0f}
        };
        assert(val.value().is_int());
        auto v = clamp(val.value().value.i, 0, 3);
        assert(val.value().value.i == v);
        color = colors[v];
      }
    }
#endif
    if (target_param.size() == 1) {
      if (auto val = ctx.ui_parameter_manager.require_and_read_value(*target_param[0])) {
        auto off = Vec3f{0.0f, 0.0f, -2.0f};

        vk::debug::draw_cube(
          node.position + off + Vec3f{0.0f, 2.0f, 0.0f} * val.value().fractional_value(),
          Vec3f{0.25f}, color);
      }
    }

    const auto writer = node.param_writer;
    auto& write_access = *param_system::ui_get_write_access(ctx.param_sys);
#if DEBUG_PARAMS
    if (!node.added_break_points && target_param.size() == 1 && freq_param.size() == 1) {
#if 1
      if (write_access.request(writer, *target_param[0])) {
        auto& desc = *target_param[0];
        auto bp0 = make_break_point(
          make_interpolated_parameter_value_from_descriptor(desc, 0.0f),
          {});
        param_system::ui_insert_break_point(ctx.param_sys, writer, globals.bp_set, desc, bp0);

        auto bp1 = make_break_point(
          make_interpolated_parameter_value_from_descriptor(desc, 1.0f),
          ScoreCursor{1, 0.0});
        param_system::ui_insert_break_point(ctx.param_sys, writer, globals.bp_set, desc, bp1);

        auto bp2 = make_break_point(
          make_interpolated_parameter_value_from_descriptor(desc, 0.5f),
          ScoreCursor{2, 0.0});
        param_system::ui_insert_break_point(ctx.param_sys, writer, globals.bp_set, desc, bp2);

        auto bp3 = make_break_point(
          make_interpolated_parameter_value_from_descriptor(desc, 1.0f),
          ScoreCursor{3, 0.0});
        param_system::ui_insert_break_point(ctx.param_sys, writer, globals.bp_set, desc, bp3);
        write_access.release(writer, target_param[0]->ids);
      }
#endif
      if (write_access.request(writer, *freq_param[0])) {
        auto& desc = *freq_param[0];

        auto bp00 = make_break_point(make_int_parameter_value(2), {});
        param_system::ui_insert_break_point(ctx.param_sys, writer, globals.bp_set, desc, bp00);

        for (int i = 0; i < 4; i++) {
          auto bp0 = make_break_point(make_int_parameter_value(1), {2, float(i * 2) * 0.25});
          param_system::ui_insert_break_point(ctx.param_sys, writer, globals.bp_set, desc, bp0);

          auto bp1 = make_break_point(make_int_parameter_value(3), {2, float(i * 2 + 1) * 0.25});
          param_system::ui_insert_break_point(ctx.param_sys, writer, globals.bp_set, desc, bp1);
        }

        write_access.release(writer, *freq_param[0]);
      }

      node.added_break_points = true;
    }
#endif
  }
#endif
}

void debug::render_debug_audio_parameter_events_gui(const DebugAudioParameterEventsContext& ctx) {
  if (!globals.initialized) {
    return;
  }

  ImGui::Begin("DebugParams");

  for (int i = 0; i < int(globals.node_info.size()); i++) {
    auto& node_info = globals.node_info[i];
    auto writer = node_info.param_writer;

    std::string label{"Node"};
    label += std::to_string(i);
    if (ImGui::TreeNode(label.c_str())) {
      Temporary<AudioParameterDescriptor, 256> tmp_descs;
      auto view_descs = tmp_descs.view_stack();
      auto params = ctx.node_storage.audio_parameter_descriptors(node_info.node_id, view_descs);
      for (auto& desc : params) {
        if (!desc.is_editable()) {
          continue;
        }

        auto& write_access = *param_system::ui_get_write_access(ctx.param_sys);
        AudioParameterWriteAccess::ScopedAccess access{write_access, writer, desc.ids};
        if (!access.acquired) {
          continue;
        }

        auto val = param_system::ui_get_set_value_or_default(ctx.param_sys, desc);
        if (val.is_float()) {
          float v = val.data.f;
          float min = desc.min.f;
          float max = desc.max.f;
          if (ImGui::SliderFloat(desc.name, &v, min, max)) {
            param_system::ui_set_value(ctx.param_sys, writer, desc.ids, make_float_parameter_value(v));
          }
        } else if (val.is_int()) {
          int v = val.data.i;
          int min = desc.min.i;
          int max = desc.max.i;
          if (ImGui::SliderInt(desc.name, &v, min, max)) {
            param_system::ui_set_value(ctx.param_sys, writer, desc.ids, make_int_parameter_value(v));
          }
        }

        if (param_system::ui_is_ui_controlled(ctx.param_sys, desc.ids)) {
          ImGui::SameLine();
          std::string rev_label{"Revert"};
          rev_label += desc.name;
          if (ImGui::SmallButton(rev_label.c_str())) {
            param_system::ui_revert_to_break_points(ctx.param_sys, writer, desc.ids);
          }
        }
      }

      ImGui::TreePop();
    }
  }

  ImGui::End();
}

GROVE_NAMESPACE_END
