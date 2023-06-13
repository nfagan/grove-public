#include "audio_node_editor.hpp"
#include "ui_common.hpp"
#include "grove/gui/gui_layout.hpp"
#include "grove/gui/gui_cursor.hpp"
#include "../render/render_gui_data.hpp"
#include "../audio_core/AudioComponent.hpp"
#include "../audio_core/UIAudioConnectionManager.hpp"
#include "../audio_core/audio_port_placement.hpp"
#include "../audio_processors/OscSwell.hpp"
#include "../audio_processors/SteerableSynth1.hpp"
#include "../audio_processors/MoogLPFilterNode.hpp"
#include "../audio_processors/BufferStoreSampler.hpp"
#include "../audio_processors/SimpleFM1.hpp"
#include "../audio_core/audio_node_attributes.hpp"
#include "grove/audio/audio_processor_nodes/OscillatorNode.hpp"
#include "grove/audio/AudioParameterSystem.hpp"
#include "grove/input/KeyTrigger.hpp"
#include "grove/input/MouseButtonTrigger.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Optional.hpp"

GROVE_NAMESPACE_BEGIN

#define BOXIDI(i) grove::gui::layout::BoxID::create(1, (i))

namespace {

using namespace ui;
using namespace gui;

struct AudioNodeEditorData;
struct PendingBox;

using CommonContext = AudioEditorCommonContext;
using ClickCallback = void(PendingBox&, AudioNodeEditorData&, const CommonContext&);

struct PendingBox {
  layout::BoxID box_id{};
  AudioNodeStorage::PortID port_id{};
  AudioNodeStorage::NodeID node_id{};
  bool port_connected{};
  bool port_optional{};
  Vec3f color{};
  Optional<RenderQuadDescriptor> quad_desc;
  ClickCallback* left_click_callback{};
  ClickCallback* left_down_callback{};
  ClickCallback* right_click_callback{};
};

struct EditorNode {
  uint32_t node_id;
  Vec3f color;
  const char* signal_repr_parameter_name;
  float signal_gain;
};

struct DraggingParameter {
  AudioParameterDescriptor desc{};
  float x0{};
  float value0{};
  float candidate_value{};
  float container_width{};
};

struct DraggableParameter {
  float value0{};
  float container_width{};
  AudioParameterDescriptor desc{};
};

struct AudioNodeEditorData {
  layout::Layout* layout{};
  std::vector<PendingBox> pending;
  std::vector<DraggableParameter> draggable_parameters;
  std::vector<EditorNode> nodes;
  std::unordered_set<AudioNodeStorage::NodeID> selected_nodes;
  Optional<AudioNodeStorage::NodeID> active_parameter_node_id;
  Optional<DraggingParameter> dragging_parameter;
  AudioParameterWriterID parameter_writer_id{AudioParameterWriteAccess::create_writer()};
};

void select_port(PendingBox& box, AudioNodeEditorData&, const CommonContext& common) {
  common.selected.insert(box.port_id);
  if (common.key_trigger.is_pressed(Key::LeftAlt)) {
    if (auto info = common.audio_component.audio_node_storage.get_port_info(box.port_id)) {
      uint32_t node_id = info.value().node_id;
      if (info.value().descriptor.is_input()) {
        ni::ui_toggle_isolating(common.audio_component.get_audio_node_isolator(), node_id, true);
      } else if (info.value().descriptor.is_output()) {
        ni::ui_toggle_isolating(common.audio_component.get_audio_node_isolator(), node_id, false);
      }
    }
  }
}

void disconnect_port(PendingBox& box, AudioNodeEditorData&, const CommonContext& common) {
  common.selected.remove(box.port_id);
  common.ui_audio_connection_manager.attempt_to_disconnect(box.port_id);
}

void select_node(PendingBox& box, AudioNodeEditorData& data, const CommonContext&) {
  data.selected_nodes.insert(box.node_id);
  data.active_parameter_node_id = box.node_id;
}

void begin_drag_param_slider(PendingBox& box, AudioNodeEditorData& data, const CommonContext& context) {
  const auto param_index = uint32_t(box.node_id);
  assert(param_index < uint32_t(data.draggable_parameters.size()));
  auto& draggable = data.draggable_parameters[param_index];

  DraggingParameter drag{};
  drag.desc = draggable.desc;
  drag.x0 = float(context.mouse_button_trigger.get_coordinates().first);
  drag.value0 = draggable.value0;
  drag.candidate_value = draggable.value0;
  drag.container_width = draggable.container_width;
  data.dragging_parameter = drag;
}

void create_osc_swell(PendingBox&, AudioNodeEditorData& data, const CommonContext& common) {
  auto* audio_component = &common.audio_component;
  auto* scale = audio_component->get_scale();
#if 0
  auto node_ctor = [scale](AudioNodeStorage::NodeID node_id) {
    return new OscSwell(node_id, scale, true);
  };
#else
  auto* param_sys = audio_component->get_parameter_system();
  auto node_ctor = [scale, param_sys](AudioNodeStorage::NodeID node_id) {
//    return new SteerableSynth1(node_id, param_sys, scale);
    return new SimpleFM1(node_id, param_sys, scale);
  };
#endif
  EditorNode node{};
  node.node_id = audio_component->audio_node_storage.create_node(
    node_ctor, make_port_descriptors_from_audio_node_ctor(node_ctor));
  node.color = Vec3f{0.25f, 0.75f, 1.0f};
#if 1
  node.signal_repr_parameter_name = "signal_representation";
#endif
  data.nodes.emplace_back() = node;
}

void create_filter(PendingBox&, AudioNodeEditorData& data, const CommonContext& common) {
  auto* audio_component = &common.audio_component;
  auto* param_sys = audio_component->get_parameter_system();
  auto node_ctor = [param_sys](AudioNodeStorage::NodeID node_id) {
    return new MoogLPFilterNode(node_id, param_sys);
  };
  EditorNode node{};
  node.node_id = audio_component->audio_node_storage.create_node(
    node_ctor, make_port_descriptors_from_audio_node_ctor(node_ctor));
  node.color = Vec3f{0.25f, 1.0f, 0.75f};
  data.nodes.emplace_back() = node;
}

void create_lfo(PendingBox&, AudioNodeEditorData& data, const CommonContext& common) {
  auto* audio_component = &common.audio_component;
  auto* param_sys = audio_component->get_parameter_system();
  auto* transport = &audio_component->audio_transport;
  auto node_ctor = [param_sys, transport](AudioNodeStorage::NodeID node_id) {
    return new OscillatorNode(node_id, param_sys, transport, 1);
  };
  EditorNode node{};
  node.node_id = audio_component->audio_node_storage.create_node(
    node_ctor, make_port_descriptors_from_audio_node_ctor(node_ctor));
  node.color = Vec3f{0.5f, 0.75f, 1.0f};
  data.nodes.emplace_back() = node;
}

void create_buffer_store_sampler(PendingBox&, AudioNodeEditorData& data, const CommonContext& common) {
  static int buff_index{};

  auto* store = common.audio_component.get_audio_buffer_store();
  AudioBufferHandle buffer_handle{};

  const char* buff_names[3]{"flute-c.wav", "piano-c.wav", "flute-c2.wav"};
  const int buff_ind = (buff_index++) % 3;

  const auto& buffs = common.audio_component.audio_buffers;
  if (auto buff = buffs.find_by_name(buff_names[buff_ind])) {
    buffer_handle = buff.value();
  }

  const auto* scale = common.audio_component.get_scale();
  auto node_ctor = [store, buffer_handle, scale](AudioNodeStorage::NodeID node_id) {
    return new BufferStoreSampler(node_id, store, buffer_handle, scale, true);
  };
  EditorNode node{};
  node.node_id = common.audio_component.audio_node_storage.create_node(
    node_ctor, make_port_descriptors_from_audio_node_ctor(node_ctor));
  node.color = Vec3f{0.75f, 0.5f, 1.0f};
  node.signal_repr_parameter_name = "signal_representation";
  data.nodes.emplace_back() = node;
}

void create_destination_node(PendingBox&, AudioNodeEditorData& data, const CommonContext& common) {
  auto& audio_component = common.audio_component;
  EditorNode node{};
  node.node_id = audio_component.ui_audio_graph_destination_nodes.create_node(
    audio_component.audio_node_storage,
    audio_component.audio_graph_component.renderer,
    audio_component.ui_audio_parameter_manager,
    audio_component.get_parameter_system(), false);
  node.color = Vec3f{0.5f, 1.0f, 0.75f};
  node.signal_repr_parameter_name = "signal_representation";
  data.nodes.emplace_back() = node;
}

void remove_deleted_nodes(AudioNodeEditorData& data, const CommonContext& context) {
  if (context.key_trigger.newly_pressed(Key::Backspace) && !data.selected_nodes.empty()) {
    auto it = data.nodes.begin();
    while (it != data.nodes.end()) {
      if (data.selected_nodes.count(it->node_id)) {
        if (data.active_parameter_node_id &&
            data.active_parameter_node_id.value() == it->node_id) {
          data.active_parameter_node_id = NullOpt{};
          data.dragging_parameter = NullOpt{};
        }
        context.audio_component.audio_connection_manager.maybe_delete_node(it->node_id);
        data.selected_nodes.erase(it->node_id);
        it = data.nodes.erase(it);
      } else {
        ++it;
      }
    }
  }
}

void gather_node_signal_values(AudioNodeEditorData& data, const CommonContext& context) {
  for (auto& node : data.nodes) {
    node.signal_gain = 1.0f;
    if (node.signal_repr_parameter_name) {
      const float interp_pow = 0.000125f;
      auto read_param = param_system::read_monitorable_parameter(
        context.audio_component.get_monitorable_parameter_system(),
        node.node_id, node.signal_repr_parameter_name, interp_pow);
      if (read_param.value) {
        node.signal_gain = read_param.interpolated_fractional_value;
      }
    }
  }
}

void update_dragging_parameter(AudioNodeEditorData& data, const CommonContext& context) {
  if (!data.dragging_parameter) {
    return;
  }

  auto& drag = data.dragging_parameter.value();
  const auto x = float(context.mouse_button_trigger.get_coordinates().first);
  float dx = x - drag.x0;
  float frac_val = clamp(dx / drag.container_width, -1.0f, 1.0f);
  float new_val = clamp01(frac_val + drag.value0);
  drag.candidate_value = new_val;
}

int num_inputs(const AudioNodeStorage::PortInfoForNode& port_info) {
  int ct{};
  for (auto& port : port_info) {
    if (port.descriptor.is_input()) {
      ct++;
    }
  }
  return ct;
}

void prepare(AudioNodeEditorData& data, const CommonContext& context) {
  if (!data.layout) {
    data.layout = gui::layout::create_layout(1);
  }

  auto* layout = data.layout;
  layout::clear_layout(layout);
  data.pending.clear();
  data.draggable_parameters.clear();

  if (context.hidden || context.mode != ui::AudioEditorMode::Node) {
    return;
  }

  remove_deleted_nodes(data, context);
  gather_node_signal_values(data, context);
  update_dragging_parameter(data, context);

  auto* cursor_state = &context.cursor_state;
  const auto* audio_component = &context.audio_component;
  const auto* node_isolator = audio_component->get_audio_node_isolator();

  auto fb_dims = context.container_dimensions;
  layout::set_root_dimensions(layout, fb_dims.x, fb_dims.y);
  layout::begin_group(layout, 0, layout::GroupOrientation::Col);
  int root = layout::box(layout, {1.0f, 50.0f, 512.0f}, {1.0f, 50.0f, 600.0f});
  layout::set_box_cursor_events(layout, root, {layout::BoxCursorEvents::Scroll});
  layout::end_group(layout);
  if (!layout::is_fully_clipped_box(layout, root)) {
    auto& drawable = data.pending.emplace_back();
    const auto color = Vec3f{0.75f, 1.0f, 0.25f};
    drawable.quad_desc = make_render_quad_desc(layout::read_box(layout, root), color, {}, {}, {}, 1.0f);
  }

  layout::begin_group(layout, root, layout::GroupOrientation::Row);
  const int node_opts = gui::layout::box(layout, {1.0f}, {0.25f});
//  layout::set_box_cursor_events(layout, node_opts, {layout::BoxCursorEvents::Scroll});
  const int node_cont = gui::layout::box(layout, {1.0f}, {0.5f + 0.125f});
  layout::set_box_cursor_events(layout, node_cont, {layout::BoxCursorEvents::Scroll});
  const int node_params = gui::layout::box(layout, {1.0f}, {0.125f});
  layout::set_box_cursor_events(layout, node_params, {layout::BoxCursorEvents::Scroll});
  layout::end_group(layout);

  {
    const int num_node_types = 5;
    ClickCallback* cbs[num_node_types] = {
      &create_osc_swell,
      &create_filter,
      &create_lfo,
      &create_buffer_store_sampler,
      &create_destination_node
    };
    const Vec3f button_colors[num_node_types] = {
      Vec3f{0.25f, 0.75f, 1.0f},
      Vec3f{0.25f, 1.0f, 0.75f},
      Vec3f{0.5f, 0.75f, 1.0f},
      Vec3f{0.75f, 0.5f, 1.0f},
      Vec3f{0.5f, 1.0f, 0.75f}
    };

    const int b0 = layout::next_box_index(layout);
    float scroll{};
    cursor::read_scroll_offsets(cursor_state, BOXIDI(node_opts), nullptr, &scroll);
    gui::layout::begin_group(layout, node_opts, layout::GroupOrientation::Col, 0, scroll);
    for (int i = 0; i < num_node_types; i++) {
      (void) gui::layout::box(layout, {1, 64, 64}, {1, 64, 64});
    }
    gui::layout::end_group(layout);

    const int sub_off = layout::next_box_index(layout);
    for (int i = 0; i < num_node_types; i++) {
      gui::layout::begin_group(layout, b0 + i, layout::GroupOrientation::Row, 0, 0, {}, {10, 10, 10, 10});
      int bi = gui::layout::box(layout, {1.0f}, {1.0f});
      gui::layout::set_box_cursor_events(layout, bi, {layout::BoxCursorEvents::Click});
      gui::layout::end_group(layout);
    }

    for (int i = 0; i < num_node_types; i++) {
      const int ind = sub_off + i;
      if (!layout::is_fully_clipped_box(layout, ind)) {
        auto& drawable = data.pending.emplace_back();
        drawable.box_id = BOXIDI(ind);
        drawable.quad_desc = make_render_quad_desc(layout::read_box(layout, ind), button_colors[i]);
        drawable.quad_desc.value().border_px = 4.0f;
        drawable.quad_desc.value().linear_border_color = Vec3f{0.75f};
        drawable.left_click_callback = cbs[i];
      }
    }
  }

  {
#if 1
    const float node_border = 4.0f;
    const float node_radius_frac = 0.0f;
    const Vec3f node_border_color{0.75f};
#else
    const float node_border = 4.0f;
    const float node_radius_frac = 0.0f;
#endif
    float scroll{};
    cursor::read_scroll_offsets(cursor_state, BOXIDI(node_cont), nullptr, &scroll);
    gui::layout::begin_group(layout, node_cont, layout::GroupOrientation::Block, 0, scroll, layout::JustifyContent::Left);

    const int cont_off = layout::next_box_index(layout);
    const int num_nodes = int(data.nodes.size());
    for (int i = 0; i < num_nodes; i++) {
      const auto& node = data.nodes[i];
      (void) node;
      auto sz = layout::BoxDimensions{1.0f, 128.0f, 128.0f};
      gui::layout::box(layout, sz, sz, false);
    }
    gui::layout::end_group(layout);

    const int sub_off = layout::next_box_index(layout);
    for (int i = 0; i < num_nodes; i++) {
      const float pad = 20.0f;
      gui::layout::begin_group(layout, cont_off + i, {}, 0, 0, {}, {pad, pad, pad, pad});
      int sub = gui::layout::box(layout, {1.0f}, {1.0f});
      gui::layout::set_box_cursor_events(layout, sub, {gui::layout::BoxCursorEvents::Click});
      gui::layout::end_group(layout);
    }

    for (int i = 0; i < num_nodes; i++) {
      auto i0 = sub_off + i;
      if (!layout::is_fully_clipped_box(layout, i0)) {
        auto& drawable = data.pending.emplace_back();
        auto& node = data.nodes[i];
        auto box = layout::read_box(layout, i0);
        drawable.box_id = BOXIDI(i0);
        drawable.node_id = node.node_id;
//        const auto color = Vec3f{1.0f, 0.125f, 0.125f};
        drawable.quad_desc = make_render_quad_desc(box, node.color, node_border, node_border_color);
        drawable.quad_desc.value().radius_fraction = node_radius_frac;
        drawable.quad_desc.value().linear_border_color *= node.signal_gain;
        drawable.left_down_callback = select_node;
      }
    }

    const int io_off = layout::next_box_index(layout);
    for (int i = 0; i < num_nodes; i++) {
      const float bw = node_border + 10.0f;
      auto i0 = sub_off + i;
      gui::layout::begin_group(layout, i0, gui::layout::GroupOrientation::Col, 0, 0, {}, {bw, bw, bw, bw});
      int in = gui::layout::box(layout, {0.5f}, {1.0f});
      gui::layout::set_box_cursor_events(layout, in, {gui::layout::BoxCursorEvents::Pass});
      int out = gui::layout::box(layout, {0.5f}, {1.0f});
      gui::layout::set_box_cursor_events(layout, out, {gui::layout::BoxCursorEvents::Pass});
      gui::layout::end_group(layout);
    }

    {
      const int in_off = layout::next_box_index(layout);
      int in_end = in_off;

      const int pend_off = int(data.pending.size());
      for (int i = 0; i < num_nodes; i++) {
        auto& node = data.nodes[i];
        const int i0 = io_off + i * 2;
        gui::layout::begin_group(layout, i0, gui::layout::GroupOrientation::Row, 0, 0, gui::layout::JustifyContent::Left);
        auto port_info = audio_component->audio_node_storage.get_port_info_for_node(node.node_id);
        assert(port_info);
        const bool has_margin = num_inputs(port_info.value()) == 2;
        for (auto& port : port_info.value()) {
          if (port.descriptor.is_input()) {
            int pi = gui::layout::box(layout, {0.25f, 20, 20}, {0.25f, 20, 20}, false);
            if (has_margin) {
              gui::layout::set_box_margin(layout, pi, 0, 0, 0, 10);
            }
            gui::layout::set_box_cursor_events(layout, pi, {gui::layout::BoxCursorEvents::Click});
            in_end = pi + 1;

            auto& pend = data.pending.emplace_back();
            pend.node_id = node.node_id;
            pend.port_id = port.id;
            pend.box_id = BOXIDI(pi);
            pend.left_click_callback = select_port;
            pend.right_click_callback = disconnect_port;
            pend.port_connected = port.connected();
            pend.port_optional = port.descriptor.is_optional();
            pend.color = color_for_data_type(port.descriptor.data_type);
          }
        }
        gui::layout::end_group(layout);
      }

      for (int i = in_off; i < in_end; i++) {
        if (!layout::is_fully_clipped_box(layout, i)) {
          auto box = layout::read_box(layout, i);
          auto& drawable = data.pending[(i - in_off) + pend_off];
          drawable.quad_desc = make_render_quad_desc(box, drawable.color, 4.0f, Vec3f{1.0f});

          if (drawable.port_connected || drawable.port_optional) {
            auto small_box = layout::evaluate_clipped_box_centered(layout, i, {0, 4, 4}, {0, 4, 4});
            if (!small_box.is_clipped()) {
              auto& next = data.pending.emplace_back();
              auto color = drawable.port_connected ? Vec3f{1.0f} : Vec3f{};
              if (ni::ui_is_isolating(node_isolator, drawable.node_id, true)) {
                color = color_for_isolating_ports();
              }
              next.quad_desc = make_render_quad_desc(small_box, color);
            }
          }
        }
      }
    }

    {
      const int out_off = layout::next_box_index(layout);
      int out_end = out_off;
      const int pend_off = int(data.pending.size());
      for (int i = 0; i < num_nodes; i++) {
        auto& node = data.nodes[i];
        const int i0 = io_off + i * 2 + 1;
        gui::layout::begin_group(layout, i0, gui::layout::GroupOrientation::Row, 0, 0, gui::layout::JustifyContent::Right);
        auto port_info = audio_component->audio_node_storage.get_port_info_for_node(node.node_id);
        assert(port_info);
        for (auto& port : port_info.value()) {
          if (port.descriptor.is_output()) {
            int pi = gui::layout::box(layout, {0.25f, 20, 20}, {0.25f, 20, 20}, false);
            gui::layout::set_box_margin(layout, pi, 0, 0, 0, 10);
            gui::layout::set_box_cursor_events(layout, pi, {gui::layout::BoxCursorEvents::Click});
            out_end = pi + 1;

            auto& pend = data.pending.emplace_back();
            pend.port_id = port.id;
            pend.node_id = port.node_id;
            pend.box_id = BOXIDI(pi);
            pend.left_click_callback = select_port;
            pend.right_click_callback = disconnect_port;
            pend.color = color_for_data_type(port.descriptor.data_type);
            pend.port_connected = port.connected();
            pend.port_optional = port.descriptor.is_optional();
          }
        }
        gui::layout::end_group(layout);
      }

      for (int i = out_off; i < out_end; i++) {
        if (!layout::is_fully_clipped_box(layout, i)) {
          auto box = layout::read_box(layout, i);
          auto& drawable = data.pending[(i - out_off) + pend_off];
          drawable.quad_desc = make_render_quad_desc(box, drawable.color);

          if (drawable.port_connected || drawable.port_optional) {
            auto small_box = layout::evaluate_clipped_box_centered(layout, i, {0, 4, 4}, {0, 4, 4});
            if (!small_box.is_clipped()) {
              auto& next = data.pending.emplace_back();
              auto color = drawable.port_connected ? Vec3f{1.0f} : Vec3f{};
              if (ni::ui_is_isolating(node_isolator, drawable.node_id, false)) {
                color = color_for_isolating_ports();
              }
              next.quad_desc = make_render_quad_desc(small_box, color);
            }
          }
        }
      }
    }
  }

  if (data.active_parameter_node_id) { //  node params
    float scroll{};
    cursor::read_scroll_offsets(cursor_state, BOXIDI(node_params), nullptr, &scroll);

    float pad_amt = 16.0f;
    auto pad = layout::GroupPadding{pad_amt, pad_amt, pad_amt, 0.0f};
    gui::layout::begin_group(layout, node_params, layout::GroupOrientation::Block, 0, scroll, layout::JustifyContent::Left, pad);

    AudioNodeStorage::NodeID target_node = data.active_parameter_node_id.value();
    Temporary<AudioParameterDescriptor, 256> store_param_descs;
    TemporaryViewStack<AudioParameterDescriptor> param_descs = store_param_descs.view_stack();
    context.audio_component.audio_node_storage.audio_parameter_descriptors(target_node, param_descs);

    int next_ind = layout::next_box_index(layout);
    int num_added{};
    for (auto& desc : param_descs) {
      if (desc.is_editable()) {
        layout::box(layout, {0.5f}, {1.0f});
        num_added++;
      }
    }
    gui::layout::end_group(layout);

    int pend_beg = int(data.pending.size());
    for (int i = 0; i < num_added; i++) {
      auto sub_pad = layout::GroupPadding{8, 16, 8, 16};
      layout::begin_group(layout, next_ind + i, layout::GroupOrientation::Col, 0, 0, {}, sub_pad);
      int next = layout::box(layout, {1.0f}, {1.0f});
      layout::end_group(layout);

      auto& pend = data.pending.emplace_back();
      pend.box_id = BOXIDI(next);
    }
    int pend_end = int(data.pending.size());

    for (int i = 0; i < (pend_end - pend_beg); i++) {
      auto& pend = data.pending[i + pend_beg];
      auto box = layout::read_box(layout, pend.box_id.index());
      if (!box.is_clipped()) {
        pend.quad_desc = ui::make_render_quad_desc(box, Vec3f{1.0f});
        pend.quad_desc.value().translucency = 0.5f;
      }
    }

    int ai{};
    for (auto& desc : param_descs) {
      if (desc.is_editable()) {
        layout::begin_group(layout, next_ind + num_added + ai, layout::GroupOrientation::Manual, 0, 0, layout::JustifyContent::None);

        float fv;
        if (data.dragging_parameter && data.dragging_parameter.value().desc.ids == desc.ids) {
          fv = data.dragging_parameter.value().candidate_value;
        } else {
          auto val = param_system::ui_get_set_value_or_default(
            context.audio_component.get_parameter_system(), desc);
          fv = val.to_float01(desc.min, desc.max);
        }

        const auto cont_box = layout::read_box(layout, next_ind + num_added + ai);
        const float handle_w = cont_box.content_height();
        const float px_span = cont_box.content_width() - handle_w;
        const float xoff = px_span * fv;

        int handle = layout::box(layout, {1, handle_w, handle_w}, {1, handle_w, handle_w});
        layout::set_box_cursor_events(layout, handle, {layout::BoxCursorEvents::Click});
        layout::set_box_offsets(layout, handle, xoff, 0);
        layout::end_group(layout);

        auto handle_box = layout::read_box(layout, handle);
        if (!handle_box.is_clipped()) {
          const uint32_t drag_ind = uint32_t(data.draggable_parameters.size());
          auto& draggable = data.draggable_parameters.emplace_back();
          draggable.container_width = px_span;
          draggable.value0 = fv;
          draggable.desc = desc;

          auto& pend = data.pending.emplace_back();
          pend.node_id = drag_ind;
          pend.box_id = BOXIDI(handle);
          pend.quad_desc = ui::make_render_quad_desc(handle_box, Vec3f{0.5f, 0.75f, 1.0f});
          pend.quad_desc.value().translucency = 0.5f;
          pend.quad_desc.value().border_px = 4.0f;
          pend.left_down_callback = begin_drag_param_slider;
        }

        ai++;
      }
    }
  }

  //  end
  const auto* read_beg = read_box_slot_begin(layout);
  cursor::evaluate_boxes(cursor_state, 1, read_beg, layout::total_num_boxes(layout));
}

void evaluate(AudioNodeEditorData& data, const CommonContext& context) {
  if (context.mouse_button_trigger.newly_pressed(Mouse::Button::Left) &&
      !context.key_trigger.is_pressed(Key::LeftControl)) {
    data.selected_nodes.clear();
  }

  const auto* cursor_state = &context.cursor_state;
  for (auto& pend : data.pending) {
    if (pend.left_click_callback && cursor::left_clicked_on(cursor_state, pend.box_id)) {
      pend.left_click_callback(pend, data, context);
    }
    if (pend.left_down_callback && cursor::newly_left_down_on(cursor_state, pend.box_id)) {
      pend.left_down_callback(pend, data, context);
    }
    if (pend.right_click_callback && cursor::right_clicked_on(cursor_state, pend.box_id)) {
      pend.right_click_callback(pend, data, context);
    }
  }

  if (data.dragging_parameter) {
    auto& drag = data.dragging_parameter.value();
    auto* write_access = param_system::ui_get_write_access(
      context.audio_component.get_parameter_system());
    if (write_access->request(data.parameter_writer_id, drag.desc)) {
      auto desired_val = make_interpolated_parameter_value_from_descriptor(
        drag.desc, drag.candidate_value);
      param_system::ui_set_value(
        context.audio_component.get_parameter_system(),
        data.parameter_writer_id, drag.desc.ids, desired_val);
      write_access->release(data.parameter_writer_id, drag.desc);
    }
    if (context.mouse_button_trigger.newly_released(Mouse::Button::Left)) {
      data.dragging_parameter = NullOpt{};
    }
  }
}

void render(AudioNodeEditorData& data, const CommonContext& context) {
  const auto* cursor_state = &context.cursor_state;
  for (auto& pend : data.pending) {
    if (pend.quad_desc) {
      if (cursor::left_down_on(cursor_state, pend.box_id) ||
          context.selected.contains(pend.port_id) ||
          data.selected_nodes.count(pend.node_id)) {
        pend.quad_desc.value().linear_color *= 0.75f;

      } else if (cursor::hovered_over(cursor_state, pend.box_id)) {
        pend.quad_desc.value().linear_color *= 0.75f;
      }
      gui::draw_quads(&context.render_data, &pend.quad_desc.value(), 1);
    }
  }
}

struct {
  AudioNodeEditorData data;
} globals;

} //  anon

void ui::prepare_audio_node_editor(const CommonContext& context) {
  prepare(globals.data, context);
}

void ui::evaluate_audio_node_editor(const CommonContext& context) {
  evaluate(globals.data, context);
}

void ui::render_audio_node_editor(const CommonContext& context) {
  render(globals.data, context);
}

void ui::destroy_audio_node_editor() {
  gui::layout::destroy_layout(&globals.data.layout);
}

#undef BOXIDI

GROVE_NAMESPACE_END
