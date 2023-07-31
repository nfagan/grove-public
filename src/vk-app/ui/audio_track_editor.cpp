#include "audio_track_editor.hpp"
#include "ui_common.hpp"
#include "gui_draw.hpp"
#include "gui_components.hpp"
#include "../render/render_gui_data.hpp"
#include "../audio_core/pitch_sampling.hpp"
#include "../audio_core/rhythm_parameters.hpp"
#include "../audio_core/control_note_clip_state_machine.hpp"
#include "../audio_core/AudioComponent.hpp"
#include "../audio_core/audio_port_placement.hpp"
#include "../audio_core/audio_node_attributes.hpp"
#include "../audio_core/UITrackSystem.hpp"
#include "grove/gui/gui_layout.hpp"
#include "grove/gui/gui_cursor.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Optional.hpp"
#include "grove/input/KeyTrigger.hpp"
#include "grove/input/MouseButtonTrigger.hpp"
#include "grove/math/random.hpp"

#define GUI_LAYOUT_ID (17)
#define BOXIDI(i) grove::gui::layout::BoxID::create(GUI_LAYOUT_ID, (i))

GROVE_NAMESPACE_BEGIN

namespace {

using namespace gui;
using namespace ui;

enum class TrackControlMode {
  NoteClipStateMachine = 0,
  Arp
};

struct AudioTrackEditorData {
  layout::Layout* layout{};
  BoxDrawList draw_list;
  elements::Elements gui_elements;
  Optional<UITrackSystemTrackHandle> selected_track;
  bool prepared{};
  Vec2f semitone_span{-12.0f, 12.0f};

  elements::SliderData slider_clip_span{};
  elements::SliderData slider_arp_num_slots{};
  elements::SliderData slider_arp_pitch_mode{};
  elements::SliderData slider_arp_duration_mode{};
  elements::SliderData slider_bpm{};
  elements::SliderData slider_scale0{};
  elements::SliderData slider_reference_semitone{};
  elements::SliderData slider_pitch_sample_group1_mode{};
  elements::SliderData slider_pitch_sample_group2_mode{};
  elements::SliderData slider_rhythm_p_quantized{};
  elements::SliderData slider_clip_randomization_note_set_index{};
  TrackControlMode track_control_mode{};
};

struct {
  AudioTrackEditorData data;
} globals;

ScoreCursor get_min_clip_size() {
  return ScoreCursor{0, 0.5};
}

ScoreCursor get_max_clip_size() {
  return ScoreCursor{4, 0.0};
}

void drag_scale0(float optf, void* ctx) {
  auto& context = *static_cast<const AudioEditorCommonContext*>(ctx);
  auto* scale_sys = context.audio_component.get_audio_scale_system();
  int opt = clamp(int(optf), 0, scale_system::ui_get_num_scales(scale_sys) - 1);
  auto curr_descs = scale_system::ui_get_active_scale_descriptors(scale_sys);
  scale_system::ui_set_scale_indices(scale_sys, opt, curr_descs.scales[1].index);

  context.pitch_sampling_parameters.refresh_note_set_indices(
    context.audio_component.get_pitch_sampling_system(),
    context.audio_component.get_audio_scale_system());
}

void drag_bpm(float value, void* ctx) {
  auto& context = *static_cast<const AudioEditorCommonContext*>(ctx);
  context.audio_component.audio_transport.set_bpm(value);
}

void drag_reference_semitone(float value, void* ctx) {
  auto& context = *static_cast<const AudioEditorCommonContext*>(ctx);
#if 0
  Tuning tuning = *context.audio_component.get_ui_scale()->get_tuning();
  tuning.reference_semitone = value;
  context.audio_component.get_ui_scale()->set_tuning(tuning);
#else
  auto* scale_sys = context.audio_component.get_audio_scale_system();
  auto tuning = *scale_system::ui_get_tuning(scale_sys);
  tuning.reference_semitone = value;
  scale_system::ui_set_tuning(scale_sys, tuning);
#endif
}

[[maybe_unused]]
void drag_pitch_sample_group1_mode(float val, void* ctx) {
  auto& context = *static_cast<const AudioEditorCommonContext*>(ctx);
  context.pitch_sampling_parameters.set_primary_note_set_index(
    context.audio_component.get_pitch_sampling_system(),
    context.audio_component.get_audio_scale_system(), int(val));
}

void drag_pitch_sample_group2_mode(float val, void* ctx) {
  auto& context = *static_cast<const AudioEditorCommonContext*>(ctx);
  context.pitch_sampling_parameters.set_secondary_note_set_index(
    context.audio_component.get_pitch_sampling_system(),
    context.audio_component.get_audio_scale_system(), int(val));
}

void drag_global_p_quantized(float v, void* ctx) {
  auto& context = *static_cast<const AudioEditorCommonContext*>(ctx);
  context.rhythm_parameters.set_global_p_quantized(v);
}

void drag_arp_pitch_mode(float value, void* ctx) {
  auto& context = *static_cast<const AudioEditorCommonContext*>(ctx);

  auto& data = globals.data;
  if (!data.selected_track) {
    return;
  }

  auto* arp_sys = context.audio_component.get_arpeggiator_system();
  auto* track = track::read_track(&context.ui_track_system, data.selected_track.value());
  assert(track);

  int pmi = int(clamp(value, 0.0f, float(ArpeggiatorSystemPitchMode::SIZE) - 1.0f));
  arp::ui_set_pitch_mode(arp_sys, track->arp, ArpeggiatorSystemPitchMode(pmi));
}

void drag_arp_duration_mode(float value, void* ctx) {
  auto& context = *static_cast<const AudioEditorCommonContext*>(ctx);

  auto& data = globals.data;
  if (!data.selected_track) {
    return;
  }

  auto* arp_sys = context.audio_component.get_arpeggiator_system();
  auto* track = track::read_track(&context.ui_track_system, data.selected_track.value());
  assert(track);

  int dmi = int(clamp(value, 0.0f, float(ArpeggiatorSystemDurationMode::SIZE) - 1.0f));
  arp::ui_set_duration_mode(arp_sys, track->arp, ArpeggiatorSystemDurationMode(dmi));
}

void drag_arp_num_active_slots(float value, void* ctx) {
  auto& context = *static_cast<const AudioEditorCommonContext*>(ctx);

  auto& data = globals.data;
  if (!data.selected_track) {
    return;
  }

  auto* arp_sys = context.audio_component.get_arpeggiator_system();
  auto* track = track::read_track(&context.ui_track_system, data.selected_track.value());
  assert(track);

  int ns = int(clamp(value, 1.0f, 4.0f));
  arp::ui_set_num_active_slots(arp_sys, track->arp, uint8_t(ns));
}

void drag_clip_size(float frac_value, void* ctx) {
  auto& context = *static_cast<const AudioEditorCommonContext*>(ctx);

  auto& data = globals.data;
  if (!data.selected_track) {
    return;
  }

  auto quant_size = audio::Quantization::Eighth;
  if (context.key_trigger.is_pressed(Key::LeftAlt)) {
    quant_size = audio::Quantization::Measure;
  }

  auto* ncsm_sys = context.audio_component.get_note_clip_state_machine_system();
  auto* clip_sys = context.audio_component.get_note_clip_system();

  auto* track = track::read_track(&context.ui_track_system, data.selected_track.value());
  auto read_voice = ncsm::ui_read_voice(ncsm_sys, track->ncsm_voice_index);
  const int si = read_voice.section;
  const auto read_section = ncsm::ui_read_section(ncsm_sys, si);

  const double tsig_num = reference_time_signature().numerator;
  double max_clip_size = get_max_clip_size().to_beats(tsig_num);
  double new_clip_size = clamp01(frac_value) * max_clip_size;

  auto curs_size = ScoreCursor::from_beats(new_clip_size, tsig_num);
  curs_size.beat = audio::quantize_floor(curs_size.beat, quant_size, tsig_num);
  curs_size = std::max(curs_size, get_min_clip_size());

  ui_set_clip_span(clip_sys, read_section.clip_handle, ScoreRegion{{}, curs_size});
}

bool is_selected(const AudioTrackEditorData& data, UITrackSystemTrackHandle handle) {
  return data.selected_track && data.selected_track.value() == handle;
}

void create_track(void* ctx) {
  auto* context = static_cast<const AudioEditorCommonContext*>(ctx);
  auto* pss = context->audio_component.get_pitch_sampling_system();
  auto pitch_sample_group = context->pitch_sampling_parameters.get_primary_group_handle(pss);
  track::create_track(&context->ui_track_system, context->audio_component, pitch_sample_group);
}

void select_track(void* ctx, const elements::StatefulButtonData& data) {
  const UITrackSystemTrackHandle handle{data.as_uint32()};
  globals.data.selected_track = handle;

  auto* context = static_cast<const AudioEditorCommonContext*>(ctx);
  track::set_midi_recording_track(&context->ui_track_system, handle);
}

void select_port(void* ctx, const elements::StatefulButtonData& data) {
  auto* context = static_cast<const AudioEditorCommonContext*>(ctx);
  auto& sel = context->selected;
  if (!context->key_trigger.is_pressed(Key::LeftControl)) {
    sel.selected_port_ids.clear();
  }

  sel.selected_port_ids.insert(data.as_uint32());
}

void play_clip(void* ctx, const elements::StatefulButtonData& data) {
  auto* context = static_cast<const AudioEditorCommonContext*>(ctx);
  auto* ncsm_sys = context->audio_component.get_note_clip_state_machine_system();
  auto* control_ncsm = &context->control_note_clip_state_machine;
  uint32_t vi{};
  uint32_t si{};
  data.as_2_uint32(&vi, &si);
  ncsm::set_next_section_index(control_ncsm, ncsm_sys, int(vi), int(si));
}

void randomize_one_clip(
  const AudioEditorCommonContext& context, NoteClipSystem* clip_sys,
  const AudioScaleSystem* scale_sys, NoteClipHandle clip_handle, int nsi) {
  //
  const ScoreCursor clip_sizes[3]{ScoreCursor{1, 0}, ScoreCursor{2, 0}, ScoreCursor{4, 0}};
  const double beat_event_intervals[6]{1.0, 1.0, 1.0, 0.5, 0.5, 0.25};
  const double p_rests[5]{0.125, 0.125, 0.125, 0.5, 0.75};
  const double tsig_num = reference_time_signature().numerator;
  auto clip_size = *uniform_array_sample(clip_sizes, 3);
  double p_rest = *uniform_array_sample(p_rests, 5);
  double event_isi = *uniform_array_sample(beat_event_intervals, 6);

  float sts[pss::PitchSamplingParameters::max_num_notes];
  int num_sts{};
  context.pitch_sampling_parameters.get_note_set(scale_sys, nsi, sts, &num_sts);
  assert(num_sts > 0);
  ui_randomize_clip_contents(
    clip_sys, clip_handle, clip_size, tsig_num, p_rest, event_isi, sts, num_sts);
}

void randomize_all_clip_contents(void* ctx) {
  auto& data = globals.data;
  auto& context = *static_cast<const AudioEditorCommonContext*>(ctx);

  auto* ncsm_sys = context.audio_component.get_note_clip_state_machine_system();
  auto* clip_sys = context.audio_component.get_note_clip_system();
  const auto* scale_sys = context.audio_component.get_audio_scale_system();
  const auto* control_ncsm_sys = &context.control_note_clip_state_machine;
  const int nsi = int(data.slider_clip_randomization_note_set_index.value);

  const int ri = ncsm::get_ui_section_range_index();
  const auto section_range = ncsm::get_section_range(control_ncsm_sys, ri);
  const int nv = ncsm::ui_get_num_voices(ncsm_sys);

  for (int vi = 0; vi < nv; vi++) {
    for (int si = section_range.begin; si < section_range.end; si++) {
      auto read_section = ncsm::ui_read_section(ncsm_sys, si);
      randomize_one_clip(context, clip_sys, scale_sys, read_section.clip_handle, nsi);
    }
  }
}

void randomize_clip_contents(void* ctx) {
  auto& data = globals.data;
  auto& context = *static_cast<const AudioEditorCommonContext*>(ctx);
  if (!data.selected_track) {
    return;
  }

  auto* ncsm_sys = context.audio_component.get_note_clip_state_machine_system();
  auto* clip_sys = context.audio_component.get_note_clip_system();
  const auto* scale_sys = context.audio_component.get_audio_scale_system();

  auto* track = track::read_track(&context.ui_track_system, data.selected_track.value());
  auto read_voice = ncsm::ui_read_voice(ncsm_sys, track->ncsm_voice_index);
  const auto read_section = ncsm::ui_read_section(ncsm_sys, read_voice.section);
  const int nsi = int(data.slider_clip_randomization_note_set_index.value);

  randomize_one_clip(context, clip_sys, scale_sys, read_section.clip_handle, nsi);
}

void clear_clip_contents(void* ctx) {
  auto& data = globals.data;
  auto& context = *static_cast<const AudioEditorCommonContext*>(ctx);
  if (data.selected_track) {
    auto* ncsm_sys = context.audio_component.get_note_clip_state_machine_system();
    auto* clip_sys = context.audio_component.get_note_clip_system();
    auto* track = track::read_track(&context.ui_track_system, data.selected_track.value());
    auto read_voice = ncsm::ui_read_voice(ncsm_sys, track->ncsm_voice_index);
    const auto read_section = ncsm::ui_read_section(ncsm_sys, read_voice.section);
    ui_remove_all_notes(clip_sys, read_section.clip_handle);
  }
}

void toggle_playing(void* ctx) {
  auto* context = static_cast<const AudioEditorCommonContext*>(ctx);
  context->audio_component.audio_transport.toggle_play_stop();
}

void toggle_metronome(void* ctx) {
  auto* context = static_cast<const AudioEditorCommonContext*>(ctx);
  metronome::ui_toggle_enabled(context->audio_component.get_metronome());
}

void toggle_midi_recording(void* ctx) {
  auto* context = static_cast<const AudioEditorCommonContext*>(ctx);
  track::toggle_midi_recording_enabled(&context->ui_track_system);
}

void toggle_ncsm_auto_advance(void* ctx) {
  auto* context = static_cast<const AudioEditorCommonContext*>(ctx);
  bool v = ncsm::get_auto_advance(&context->control_note_clip_state_machine);
  ncsm::set_auto_advance(&context->control_note_clip_state_machine, !v);
}

void toggle_midi_output(
  void* ctx, const elements::StatefulButtonData& data, UITrackSystemTrack::MIDIOutputSource source) {
  //
  auto* context = static_cast<const AudioEditorCommonContext*>(ctx);
  auto* track_sys = &context->ui_track_system;
  track::toggle_midi_output_enabled(
    track_sys, context->audio_component, UITrackSystemTrackHandle{data.as_uint32()}, source);
}

void toggle_triggered_midi_output(void* ctx, const elements::StatefulButtonData& data) {
  toggle_midi_output(ctx, data, UITrackSystemTrack::MIDIOutputSource::Triggered);
}

void toggle_arp_midi_output(void* ctx, const elements::StatefulButtonData& data) {
  toggle_midi_output(ctx, data, UITrackSystemTrack::MIDIOutputSource::Arp);
}

void toggle_ncsm_midi_output(void* ctx, const elements::StatefulButtonData& data) {
  toggle_midi_output(ctx, data, UITrackSystemTrack::MIDIOutputSource::NoteClipStateMachine);
}

void set_track_mode_ncsm(void*) {
  globals.data.track_control_mode = TrackControlMode::NoteClipStateMachine;
}

void set_track_mode_arp(void*) {
  globals.data.track_control_mode = TrackControlMode::Arp;
}

void prepare_midi_output_nodes(
  AudioTrackEditorData& data, const AudioEditorCommonContext& context,
  const UITrackSystemTrack& track, int container) {
  //
  auto* layout = data.layout;
  layout::begin_group(layout, container, layout::GroupOrientation::Col);

  auto* stream_nodes = context.audio_component.get_ui_midi_message_stream_nodes();
  const int bo = layout::total_num_boxes(layout);

  for (auto node_it = stream_nodes->begin_list(track.midi_stream_nodes);
       node_it != stream_nodes->end_list(); ++node_it) {
    int bi = layout::box(layout, {1, 16, 16}, {1, 16, 16});
    layout::set_box_cursor_events(layout, bi, {layout::BoxCursorEvents::Click});
  }

  layout::end_group(layout);

  int ind{};
  for (auto node_it = stream_nodes->begin_list(track.midi_stream_nodes);
       node_it != stream_nodes->end_list(); ++node_it) {
    const int bi = bo + ind++;

    auto port_info = context.audio_component.audio_node_storage.get_port_info_for_node(*node_it);
    if (port_info && port_info.value().size() == 1) {
      auto port_id = port_info.value()[0].id;
      elements::push_stateful_button(
        &data.gui_elements, bi, elements::StatefulButtonData::from_uint32(port_id), select_port);

      auto color = color_for_data_type(port_info.value()[0].descriptor.data_type);
      if (context.selected.contains(port_id)) {
        color *= 0.5f;
      }
      draw_box(data.draw_list, layout, bi, ui::make_render_quad_desc_style(color));

      if (port_info.value()[0].connected()) {
        auto read_box = layout::evaluate_clipped_box_centered(layout, bi, {0.5f}, {0.5f});
        if (!read_box.is_clipped()) {
          auto& pend = data.draw_list.drawables.emplace_back();
          pend.quad_desc = ui::make_render_quad_desc(read_box, Vec3f{1.0f});
          pend.set_manually_positioned();
        }
      }
    }
  }
}

void prepare_midi_listeners(
  AudioTrackEditorData& data, const AudioEditorCommonContext& context,
  const UITrackSystemTrack& track, int container) {
  //
  auto* layout = data.layout;
  layout::begin_group(layout, container, layout::GroupOrientation::Col);

  int triggered_midi = layout::box(layout, {1, 16, 16}, {1, 16, 16});
  layout::set_box_cursor_events(layout, triggered_midi, {layout::BoxCursorEvents::Click});
  int arp_midi = layout::box(layout, {1, 16, 16}, {1, 16, 16});
  layout::set_box_cursor_events(layout, arp_midi, {layout::BoxCursorEvents::Click});
  int ncsm_midi = layout::box(layout, {1, 16, 16}, {1, 16, 16});
  layout::set_box_cursor_events(layout, ncsm_midi, {layout::BoxCursorEvents::Click});

  layout::end_group(layout);

  auto triggered_midi_color = color_for_data_type(AudioNodeStorage::DataType::MIDIMessage);
  triggered_midi_color *= track.triggered_midi_output_enabled(context.audio_component) ? 0.5f : 1.0f;

  auto arp_color = Vec3f{0.0f, 1.0f, 0.0f} * (track.arp_midi_output_enabled(context.audio_component) ? 0.5f : 1.0f);
  auto ncsm_color = Vec3f{0.0f, 0.0f, 1.0f} * (track.ncsm_midi_output_enabled(context.audio_component) ? 0.5f : 1.0f);

  draw_box(data.draw_list, layout, triggered_midi, ui::make_render_quad_desc_style(triggered_midi_color, 0, {}, 1));
  draw_box(data.draw_list, layout, arp_midi, ui::make_render_quad_desc_style(arp_color, 0, {}, 1));
  draw_box(data.draw_list, layout, ncsm_midi, ui::make_render_quad_desc_style(ncsm_color, 0, {}, 1));

  auto state_data = elements::StatefulButtonData::from_uint32(track.handle.id);
  elements::push_stateful_button(&data.gui_elements, triggered_midi, state_data, toggle_triggered_midi_output);
  elements::push_stateful_button(&data.gui_elements, arp_midi, state_data, toggle_arp_midi_output);
  elements::push_stateful_button(&data.gui_elements, ncsm_midi, state_data, toggle_ncsm_midi_output);
}

void prepare_track_header(
  AudioTrackEditorData& data, const AudioEditorCommonContext& context,
  const UITrackSystemTrack& track, int container) {
  //
  auto* layout = data.layout;
  layout::begin_group(layout, container, layout::GroupOrientation::Col);
  const int section0 = layout::box(layout, {0.5f}, {1});
  layout::set_box_cursor_events(layout, section0, {layout::BoxCursorEvents::Pass});
  layout::end_group(layout);

  layout::begin_group(layout, section0, layout::GroupOrientation::Row);
  int row0 = layout::box(layout, {1}, {0.5f});
  layout::set_box_cursor_events(layout, row0, {layout::BoxCursorEvents::Pass});
  int row1 = layout::box(layout, {1}, {0.5f});
  layout::set_box_cursor_events(layout, row1, {layout::BoxCursorEvents::Pass});
  layout::end_group(layout);

  prepare_midi_output_nodes(data, context, track, row0);
  prepare_midi_listeners(data, context, track, row1);
}

void prepare_track_mode_selector(
  AudioTrackEditorData& data, int cont, const AudioEditorCommonContext&) {
  //
  auto* layout = data.layout;
  layout::begin_group(layout, cont, layout::GroupOrientation::Col);
  int mode_ncsm = layout::box(layout, {1, 16, 16}, {1, 16, 16});
  layout::set_box_is_clickable(layout, mode_ncsm);
  int mode_arp = layout::box(layout, {1, 16, 16}, {1, 16, 16});
  layout::set_box_is_clickable(layout, mode_arp);
  layout::end_group(layout);

  float ncsm_border = data.track_control_mode == TrackControlMode::NoteClipStateMachine ? 2.0f : 0.0f;
  float arp_border = data.track_control_mode == TrackControlMode::Arp ? 2.0f : 0.0f;
  draw_box(data.draw_list, layout, mode_ncsm, ui::make_render_quad_desc_style(Vec3f{0.0f, 0.0f, 1.0f}, ncsm_border));
  draw_box(data.draw_list, layout, mode_arp, ui::make_render_quad_desc_style(Vec3f{0.0f, 1.0f, 0.0f}, arp_border));

  elements::push_button(&data.gui_elements, mode_ncsm, set_track_mode_ncsm);
  elements::push_button(&data.gui_elements, mode_arp, set_track_mode_arp);
}

void prepare_arp_control(
  AudioTrackEditorData& data, int cont, const AudioEditorCommonContext& context) {
  //
  assert(data.selected_track);

  auto* ui_sys = &context.ui_track_system;
  auto* arp_sys = context.audio_component.get_arpeggiator_system();
  auto* track = track::read_track(ui_sys, data.selected_track.value());
  if (!track) {
    return;
  }

  auto arp_state = arp::ui_read_state(arp_sys, track->arp);

  auto* layout = data.layout;
  layout::begin_group(layout, cont, layout::GroupOrientation::Row);
  int pitch_mode_cont = layout::box(layout, {1}, {1, 24, 24});
  int duration_mode_cont = layout::box(layout, {1}, {1, 24, 24});
  int ns_cont = layout::box(layout, {1}, {1, 24, 24});
  layout::end_group(layout);

  data.slider_arp_pitch_mode.min_value = 0;
  data.slider_arp_pitch_mode.max_value = float(ArpeggiatorSystemPitchMode::SIZE) - 1.0f;
  data.slider_arp_pitch_mode.value = float(arp_state.pitch_mode);
  data.slider_arp_pitch_mode.set_stepped(true);
  data.slider_arp_pitch_mode.step_value = 1;
  auto pm_res = prepare_simple_slider(
    data.gui_elements, &data.slider_arp_pitch_mode, layout, pitch_mode_cont,
    {1}, {1, 16, 16}, {1, 32, 32}, &context.cursor_state, drag_arp_pitch_mode);

  data.slider_arp_duration_mode.min_value = 0;
  data.slider_arp_duration_mode.max_value = float(ArpeggiatorSystemDurationMode::SIZE) - 1.0f;
  data.slider_arp_duration_mode.value = float(arp_state.duration_mode);
  data.slider_arp_duration_mode.set_stepped(true);
  data.slider_arp_duration_mode.step_value = 1;
  auto dm_res = prepare_simple_slider(
    data.gui_elements, &data.slider_arp_duration_mode, layout, duration_mode_cont,
    {1}, {1, 16, 16}, {1, 32, 32}, &context.cursor_state, drag_arp_duration_mode);

  data.slider_arp_num_slots.min_value = 1;
  data.slider_arp_num_slots.max_value = 4;
  data.slider_arp_num_slots.value = float(arp_state.num_slots_active);
  data.slider_arp_num_slots.set_stepped(true);
  data.slider_arp_num_slots.step_value = 1;
  auto ns_res = prepare_simple_slider(
    data.gui_elements, &data.slider_arp_num_slots, layout, ns_cont,
    {1}, {1, 16, 16}, {1, 32, 32}, &context.cursor_state, drag_arp_num_active_slots);

  auto track_style = ui::make_render_quad_desc_style(Vec3f{1.0f}, 0, {}, 0, 0.5f);
  draw_box(data.draw_list, layout, pm_res.slider_section, track_style);
  draw_box(data.draw_list, layout, pm_res.handle, ui::make_render_quad_desc_style(Vec3f{1.0f}, 2.0f));

  draw_box(data.draw_list, layout, dm_res.slider_section, track_style);
  draw_box(data.draw_list, layout, dm_res.handle, ui::make_render_quad_desc_style(Vec3f{1.0f}, 2.0f));

  draw_box(data.draw_list, layout, ns_res.slider_section, track_style);
  draw_box(data.draw_list, layout, ns_res.handle, ui::make_render_quad_desc_style(Vec3f{1.0f}, 2.0f));
}

void prepare_ncsm_control(
  AudioTrackEditorData& data, int cont, const AudioEditorCommonContext& context) {
  //
  assert(data.selected_track);
  auto* ui_sys = &context.ui_track_system;
  auto* track = track::read_track(ui_sys, data.selected_track.value());
  assert(track);
  const auto control_ncsm_voice = ncsm::read_voice(
    &context.control_note_clip_state_machine, track->ncsm_voice_index);
  const bool allow_clip_mod = control_ncsm_voice.section_range_index == 0;

  auto* layout = data.layout;

  layout::begin_group(layout, cont, layout::GroupOrientation::Row);
  int section0 = layout::box(layout, {1}, {0.5f});
  int section1 = layout::box(layout, {1}, {0.5f});
  layout::end_group(layout);

  layout::begin_group(layout, section0, layout::GroupOrientation::Col);
  int toggle_rec = layout::box(layout, {1, 32, 32}, {1, 32, 32});
  layout::set_box_is_clickable(layout, toggle_rec);
  int randomize_clip = layout::box(layout, {1, 32, 32}, {1, 32, 32});
  layout::set_box_is_clickable(layout, randomize_clip);
  int randomize_all_clips = layout::box(layout, {1, 32, 32}, {1, 32, 32});
  layout::set_box_is_clickable(layout, randomize_all_clips);
  int clear_clip = layout::box(layout, {1, 32, 32}, {1, 32, 32});
  layout::set_box_is_clickable(layout, clear_clip);
  layout::end_group(layout);

  if (allow_clip_mod) { //  toggle recording
    auto color = Vec3f{1.0f, 0.0f, 0.0f} * (track::is_midi_recording_enabled(ui_sys) ? 0.5f : 1.0f);
    draw_box(data.draw_list, layout, toggle_rec, ui::make_render_quad_desc_style(color));
    elements::push_button(&data.gui_elements, toggle_rec, toggle_midi_recording);
  }

  if (allow_clip_mod) { //  randomize clip
    auto color = Vec3f{0.0f, 0.0f, 1.0f};
    draw_box(data.draw_list, layout, randomize_clip, ui::make_render_quad_desc_style(color));
    elements::push_button(&data.gui_elements, randomize_clip, randomize_clip_contents);
  }

  if (allow_clip_mod) { //  randomize all clips
    auto color = Vec3f{0.0f, 1.0f, 1.0f};
    draw_box(data.draw_list, layout, randomize_all_clips, ui::make_render_quad_desc_style(color));
    elements::push_button(&data.gui_elements, randomize_all_clips, randomize_all_clip_contents);
  }

  if (allow_clip_mod) { //  clear clip
    auto color = Vec3f{};
    draw_box(data.draw_list, layout, clear_clip, ui::make_render_quad_desc_style(color));
    elements::push_button(&data.gui_elements, clear_clip, clear_clip_contents);
  }

  data.slider_clip_randomization_note_set_index.min_value = float(
    pss::PitchSamplingParameters::min_note_set_index());
  data.slider_clip_randomization_note_set_index.max_value = float(
    pss::PitchSamplingParameters::max_note_set_index());
  data.slider_clip_randomization_note_set_index.set_stepped(true);
  data.slider_clip_randomization_note_set_index.step_value = 1;
  auto pm_res = prepare_simple_slider(
    data.gui_elements, &data.slider_clip_randomization_note_set_index, layout, section1,
    {1}, {1, 16, 16}, {1, 32, 32}, &context.cursor_state, nullptr);

  auto track_style = ui::make_render_quad_desc_style(Vec3f{1.0f}, 0, {}, 0, 0.5f);
  draw_box(data.draw_list, layout, pm_res.slider_section, track_style);
  draw_box(data.draw_list, layout, pm_res.handle, ui::make_render_quad_desc_style(Vec3f{1.0f}, 2.0f));
}

void prepare_clip_length_slider(
  AudioTrackEditorData& data, int cont, const AudioEditorCommonContext& context) {
  //
  assert(data.selected_track);
  auto* ncsm_sys = context.audio_component.get_note_clip_state_machine_system();
  auto* clip_sys = context.audio_component.get_note_clip_system();

  auto* track = track::read_track(&context.ui_track_system, data.selected_track.value());
  auto read_voice = ncsm::ui_read_voice(ncsm_sys, track->ncsm_voice_index);
  const int si = read_voice.section;

  auto* layout = data.layout;

  const auto read_section = ncsm::ui_read_section(ncsm_sys, si);
  const auto* clip = ui_read_clip(clip_sys, read_section.clip_handle);
  assert(clip);

  const double tsig_num = reference_time_signature().numerator;
  double max_clip_size = get_max_clip_size().to_beats(tsig_num);
  double curr_clip_size = std::min(clip->span.size.to_beats(tsig_num), max_clip_size);
  auto frac_sz = curr_clip_size / max_clip_size;

  auto quant_size = audio::Quantization::Eighth;
  if (context.key_trigger.is_pressed(Key::LeftAlt)) {
    quant_size = audio::Quantization::Measure;
  }
  double quant_frac = 1.0 / audio::quantization_divisor(quant_size) * tsig_num / max_clip_size;

  data.slider_clip_span.min_value = float(get_min_clip_size().to_beats(tsig_num) / max_clip_size);
  data.slider_clip_span.max_value = 1.0f;
  data.slider_clip_span.set_stepped(true);
  data.slider_clip_span.step_value = float(quant_frac);
  data.slider_clip_span.value = float(frac_sz);
  auto prep_res = prepare_simple_slider(
    data.gui_elements, &data.slider_clip_span,
    layout, cont, {1}, {1, 16, 16}, {1, 32, 32}, &context.cursor_state, drag_clip_size);

  draw_box(data.draw_list, layout, prep_res.slider_section, ui::make_render_quad_desc_style(Vec3f{1.0f}, 0, {}, 0, 0.5f));
  draw_box(data.draw_list, layout, prep_res.handle, ui::make_render_quad_desc_style(Vec3f{1.0f}, 2.0f));
}

void prepare_clip(AudioTrackEditorData& data, int cont, const AudioEditorCommonContext& context) {
  assert(data.selected_track);
  auto* ncsm_sys = context.audio_component.get_note_clip_state_machine_system();
  auto* clip_sys = context.audio_component.get_note_clip_system();

  auto* track = track::read_track(&context.ui_track_system, data.selected_track.value());
  auto read_voice = ncsm::ui_read_voice(ncsm_sys, track->ncsm_voice_index);
  const int si = read_voice.section;

  const auto read_section = ncsm::ui_read_section(ncsm_sys, si);
  const auto* clip = ui_read_clip(clip_sys, read_section.clip_handle);
  assert(clip);

  auto max_clip_size = get_max_clip_size();
  auto clip_size = std::min(max_clip_size, clip->span.size);
  const double tsig_num = reference_time_signature().numerator;
  const double clip_size_beats = clip_size.to_beats(tsig_num);
  const double max_clip_size_beats = max_clip_size.to_beats(tsig_num);
  (void) clip_size_beats;

  constexpr int num_stack_notes = 1024;
  Temporary<ClipNote, num_stack_notes> store_clip_notes;
  Temporary<uint32_t, num_stack_notes> store_clip_note_inds;
  auto* clip_notes = store_clip_notes.require(num_stack_notes);
  auto* clip_note_inds = store_clip_note_inds.require(num_stack_notes);

  ScoreRegion sel_region{{}, clip_size};
  int num_notes = ui_collect_notes_intersecting_region(
    clip_sys, clip, sel_region,
    clip_note_inds, clip_notes, num_stack_notes);

  if (num_notes > num_stack_notes) {
    clip_notes = store_clip_notes.require(num_notes);
    clip_note_inds = store_clip_note_inds.require(num_notes);
    ui_collect_notes_intersecting_region(
      clip_sys, clip, sel_region, clip_note_inds, clip_notes, num_stack_notes);
  }

  auto* layout = data.layout;

  auto cont_box = layout::read_box(layout, cont);
  float cont_w = cont_box.content_width();
  float cont_h = cont_box.content_height();

  float scroll_y{};
  cursor::read_scroll_offsets(&context.cursor_state, BOXIDI(cont), nullptr, &scroll_y);
  layout::begin_group(layout, cont, layout::GroupOrientation::Manual, 0, scroll_y, layout::JustifyContent::None);

  const float min_st = data.semitone_span.x;
  const float max_st = data.semitone_span.y;
  const float st_span = max_st - min_st;

  const int box_off = layout::total_num_boxes(layout);
  for (int i = 0; i < num_notes; i++) {
    auto beg = std::max(0.0, clip_notes[i].span.begin.to_beats(tsig_num));
    auto sz = std::min(max_clip_size_beats, clip_notes[i].span.size.to_beats(tsig_num));
    auto frac_x0 = float(beg / max_clip_size_beats);
    float frac_x1 = frac_x0 + float(sz / max_clip_size_beats);
    auto frac_y0 = float((clip_notes[i].note.semitone() - min_st) / st_span);
    float frac_y1 = frac_y0 + 1.0f / st_span;
    float px_w = (frac_x1 - frac_x0) * cont_w;
    float px_h = (frac_y1 - frac_y0) * cont_h;
    const int note_box = layout::box(layout, {1, px_w, px_w}, {1, px_h, px_h});
    layout::set_box_offsets(layout, note_box, frac_x0 * cont_w, (1.0f - frac_y0) * cont_h);
    layout::set_box_cursor_events(layout, note_box, {layout::BoxCursorEvents::Click});
  }

  int cursor_box;
  {
    const float cursor_w = 2.0f;
    cursor_box = layout::box(layout, {1, cursor_w, cursor_w}, {1});
    auto cursor_p = std::min(
      max_clip_size_beats, clip->span.loop(read_voice.position, tsig_num).to_beats(tsig_num));
    cursor_p = cursor_p / max_clip_size_beats;
    layout::set_box_offsets(layout, cursor_box, float(cursor_p * cont_w), 0);
  }

  int clip_end_box;
  {
    const float clip_end_w = 2.0f;
    clip_end_box = layout::box(layout, {1, clip_end_w, clip_end_w}, {1});
    auto cursor_p = clip_size_beats / max_clip_size_beats;
    layout::set_box_offsets(layout, clip_end_box, float(cursor_p * cont_w), 0);
  }

  layout::end_group(layout);

  for (int i = 0; i < num_notes; i++) {
    draw_box(data.draw_list, layout, i + box_off, ui::make_render_quad_desc_style(Vec3f{1.0f}));
  }

  draw_box(data.draw_list, layout, cursor_box, ui::make_render_quad_desc_style(Vec3f{1.0f, 0.0f, 0.0f}));
  draw_box(data.draw_list, layout, clip_end_box, ui::make_render_quad_desc_style(Vec3f{1.0f}));
}

void prepare_header(AudioTrackEditorData& data, int cont, const AudioEditorCommonContext& context) {
  auto* layout = data.layout;
  const float cont_h = layout::read_box(layout, cont).content_height();

  layout::begin_group(layout, cont, layout::GroupOrientation::Col);
  int play_button = layout::box(layout, {1, cont_h, cont_h}, {1});
  layout::set_box_is_clickable(layout, play_button);

  int metronome_button = layout::box(layout, {1, cont_h, cont_h}, {1});
  layout::set_box_is_clickable(layout, metronome_button);

  int ncsm_auto_advance_button = layout::box(layout, {1, cont_h, cont_h}, {1});
  layout::set_box_is_clickable(layout, ncsm_auto_advance_button);

  int bpm_slider = layout::box(layout, {1, 128, 128}, {1});
  int ref_st_slider = layout::box(layout, {1, 128, 128}, {1});
  int pss_group1_slider = layout::box(layout, {1, 64, 64}, {1});
  int pss_group2_slider = layout::box(layout, {1, 64, 64}, {1});
  int p_quantized_slider = layout::box(layout, {1, 128, 128}, {1});
  layout::end_group(layout);

  bool playing = context.audio_component.audio_transport.ui_playing();
  auto play_color = Vec3f{1.0f, 0.0f, 0.0f} * (playing ? 0.5f : 1.0f);
  elements::push_button(&data.gui_elements, play_button, toggle_playing);
  draw_box(data.draw_list, layout, play_button, ui::make_render_quad_desc_style(play_color, 2.0f));

  bool metronome_enabled = metronome::ui_is_enabled(context.audio_component.get_metronome());
  auto metronome_color = Vec3f{1.0f} * (metronome_enabled ? 0.5f : 1.0f);
  elements::push_button(&data.gui_elements, metronome_button, toggle_metronome);
  draw_box(data.draw_list, layout, metronome_button, ui::make_render_quad_desc_style(metronome_color, 2.0f));

  bool ncsm_auto_advances = ncsm::get_auto_advance(&context.control_note_clip_state_machine);
  auto auto_advance_color = Vec3f{0.0f, 1.0f, 0.0f} * (ncsm_auto_advances ? 0.5f : 1.0f);
  elements::push_button(&data.gui_elements, ncsm_auto_advance_button, toggle_ncsm_auto_advance);
  draw_box(data.draw_list, layout, ncsm_auto_advance_button, ui::make_render_quad_desc_style(auto_advance_color, 2.0f));

  data.slider_bpm.min_value = 20;
  data.slider_bpm.max_value = 240;
  data.slider_bpm.value = float(context.audio_component.audio_transport.get_bpm());
  data.slider_bpm.set_stepped(true);
  data.slider_bpm.step_value = 0.5;
  auto bpm_res = prepare_simple_slider(
    data.gui_elements, &data.slider_bpm, layout, bpm_slider,
    {1}, {1, 16, 16}, {1, 16, 16}, &context.cursor_state, drag_bpm);
  draw_box(data.draw_list, layout, bpm_res.slider_section, ui::make_render_quad_desc_style(Vec3f{1.0f, 0.75f, 0.75f}));
  draw_box(data.draw_list, layout, bpm_res.handle, ui::make_render_quad_desc_style(Vec3f{1.0f}, 2.0f));

#if 0
  auto* tuning = context.audio_component.get_ui_scale()->get_tuning();
#else
  auto* scale_sys = context.audio_component.get_audio_scale_system();
#endif
  data.slider_reference_semitone.min_value = 40;
  data.slider_reference_semitone.max_value = 80;
  data.slider_reference_semitone.value = float(scale_system::ui_get_tuning(scale_sys)->reference_semitone);
  data.slider_reference_semitone.set_stepped(true);
  data.slider_reference_semitone.step_value = 1;
  auto ref_st_res = prepare_simple_slider(
    data.gui_elements, &data.slider_reference_semitone, layout, ref_st_slider,
    {1}, {1, 16, 16}, {1, 16, 16}, &context.cursor_state, drag_reference_semitone);
  draw_box(data.draw_list, layout, ref_st_res.slider_section, ui::make_render_quad_desc_style(Vec3f{0.75f, 0.75f, 1.0f}));
  draw_box(data.draw_list, layout, ref_st_res.handle, ui::make_render_quad_desc_style(Vec3f{1.0f}, 2.0f));

#if 0
  data.slider_pitch_sample_group1_mode.min_value = 0;
  data.slider_pitch_sample_group1_mode.max_value = float(
    pss::PitchSamplingParameters::max_note_set_index());
  data.slider_pitch_sample_group1_mode.value = float(
    context.pitch_sampling_parameters.primary_note_set_index);
  data.slider_pitch_sample_group1_mode.set_stepped(true);
  data.slider_pitch_sample_group1_mode.step_value = 1;
  auto pss_group1_res = prepare_simple_slider(
    data.gui_elements, &data.slider_pitch_sample_group1_mode, layout, pss_group1_slider,
    {1}, {1, 16, 16}, {1, 16, 16}, &context.cursor_state, drag_pitch_sample_group1_mode);
  draw_box(data.draw_list, layout, pss_group1_res.slider_section, ui::make_render_quad_desc_style(Vec3f{0.75f, 1.0f, 0.75f}));
  draw_box(data.draw_list, layout, pss_group1_res.handle, ui::make_render_quad_desc_style(Vec3f{1.0f}, 2.0f));
#else
  const auto scale_descs = scale_system::ui_get_active_scale_descriptors(scale_sys);

  data.slider_scale0.min_value = 0;
  data.slider_scale0.max_value = float(scale_system::ui_get_num_scales(scale_sys) - 1);
  data.slider_scale0.value = float(scale_descs.scales[0].index);
  data.slider_scale0.set_stepped(true);
  data.slider_scale0.step_value = 1;
  auto pss_group1_res = prepare_simple_slider(
    data.gui_elements, &data.slider_scale0, layout, pss_group1_slider,
    {1}, {1, 16, 16}, {1, 16, 16}, &context.cursor_state, drag_scale0);
  draw_box(data.draw_list, layout, pss_group1_res.slider_section, ui::make_render_quad_desc_style(Vec3f{0.75f, 1.0f, 0.75f}));
  draw_box(data.draw_list, layout, pss_group1_res.handle, ui::make_render_quad_desc_style(Vec3f{1.0f}, 2.0f));
#endif

  data.slider_pitch_sample_group2_mode.min_value = 0;
  data.slider_pitch_sample_group2_mode.max_value = float(
    pss::PitchSamplingParameters::max_note_set_index());
  data.slider_pitch_sample_group2_mode.value = float(
    context.pitch_sampling_parameters.secondary_note_set_index);
  data.slider_pitch_sample_group2_mode.set_stepped(true);
  data.slider_pitch_sample_group2_mode.step_value = 1;
  auto pss_group2_res = prepare_simple_slider(
    data.gui_elements, &data.slider_pitch_sample_group2_mode, layout, pss_group2_slider,
    {1}, {1, 16, 16}, {1, 16, 16}, &context.cursor_state, drag_pitch_sample_group2_mode);
  draw_box(data.draw_list, layout, pss_group2_res.slider_section, ui::make_render_quad_desc_style(Vec3f{1.0f, 1.0f, 0.75f}));
  draw_box(data.draw_list, layout, pss_group2_res.handle, ui::make_render_quad_desc_style(Vec3f{1.0f}, 2.0f));

  data.slider_rhythm_p_quantized.min_value = 0.0f;
  data.slider_rhythm_p_quantized.max_value = 1.0f;
  data.slider_rhythm_p_quantized.value = context.rhythm_parameters.global_p_quantized;
  auto rhythm_p_res = prepare_simple_slider(
    data.gui_elements, &data.slider_rhythm_p_quantized, layout, p_quantized_slider,
    {1}, {1, 16, 16}, {1, 16, 16}, &context.cursor_state, drag_global_p_quantized);
  draw_box(data.draw_list, layout, rhythm_p_res.slider_section, ui::make_render_quad_desc_style(Vec3f{1.0f, 0.75f, 1.0f}));
  draw_box(data.draw_list, layout, rhythm_p_res.handle, ui::make_render_quad_desc_style(Vec3f{1.0f}, 2.0f));
}

void prepare_clips(
  AudioTrackEditorData& data, int clip_cont, const AudioEditorCommonContext& context,
  float track_row_height, float track_row_margin) {
  //
  auto* ui_track_sys = &context.ui_track_system;
  auto* ncsm_sys = context.audio_component.get_note_clip_state_machine_system();
  auto* control_ncsm = &context.control_note_clip_state_machine;

  const auto tracks = track::read_tracks(ui_track_sys);
  const int num_tracks = int(tracks.size());
  const int num_ncsm_cols = ncsm::get_num_sections_per_range(&context.control_note_clip_state_machine);

  auto* layout = data.layout;
  layout::begin_group(layout, clip_cont, layout::GroupOrientation::Row, 0, 0, layout::JustifyContent::None);
  DynamicArray<int, 64> clip_rows;
  for (int i = 0; i < num_tracks; i++) {
    const int cr = layout::box(layout, {1}, {1, track_row_height, track_row_height});
    layout::set_box_margin(layout, cr, 0, i == 0 ? track_row_margin : 0, 0, track_row_margin);
    clip_rows.push_back(cr);
  }
  layout::end_group(layout);

  for (int t = 0; t < num_tracks; t++) {
    const auto& track = tracks[t];
    const auto read_voice_info = ncsm::ui_read_voice(ncsm_sys, track.ncsm_voice_index);

    layout::begin_group(layout, clip_rows[t], layout::GroupOrientation::Col);
    DynamicArray<int, 128> clip_cols;
    for (int i = 0; i < num_ncsm_cols; i++) {
      const int clip_box = layout::box(layout, {1, track_row_height, track_row_height}, {1});
      clip_cols.push_back(clip_box);
    }
    layout::end_group(layout);

    const auto control_ncsm_voice = ncsm::read_voice(control_ncsm, track.ncsm_voice_index);
    const auto control_ncsm_range = ncsm::get_section_range(
      control_ncsm, control_ncsm_voice.section_range_index);

    for (int i = 0; i < num_ncsm_cols; i++) {
      layout::begin_group(layout, clip_cols[i], layout::GroupOrientation::Col);
      const int clip_box = layout::box(layout, {0.75f}, {0.75f});
      clip_cols[i] = clip_box;
      layout::end_group(layout);

      if (control_ncsm_voice.section_range_index == 0) {
        layout::set_box_cursor_events(layout, clip_box, {layout::BoxCursorEvents::Click});

        const auto vi = uint32_t(track.ncsm_voice_index);
        const auto si = uint32_t(i);  //  section
        elements::push_stateful_button(
          &data.gui_elements, clip_box,
          elements::StatefulButtonData::from_2_uint32(vi, si), play_clip);
      }
    }

    const bool is_env_controlled = control_ncsm_voice.section_range_index == 1;

    for (int i = 0; i < num_ncsm_cols; i++) {
      auto style = ui::make_render_quad_desc_style(Vec3f{1.0f});
      const int abs_si = control_ncsm_range.absolute_section_index(i);
      const bool is_active = abs_si == read_voice_info.section;
      const bool is_pending = read_voice_info.next_section &&
        read_voice_info.next_section.value() == abs_si;

      if (is_active) {
        style.linear_color = Vec3f{1.0f, 0.0f, 0.0f};
        if (is_env_controlled) {
          style.linear_color = Vec3f{1.0f, 1.0f, 0.0f};
        }
      } else if (is_pending) {
        style.linear_color = Vec3f{0.0f, 0.0f, 1.0f};
      } else {
        style.translucency = 0.5f;
      }

      bool do_draw = is_env_controlled ? (is_pending || is_active) : true;
      if (do_draw) {
        auto* pend = gui::draw_box(data.draw_list, layout, clip_cols[i], style);
        if (pend && !is_pending && !is_active) {
          pend->set_small_unless_hovered();
        }
      }
    }
  }
}

void prepare_tracks(
  AudioTrackEditorData& data, int cont, const AudioEditorCommonContext& context,
  float track_row_height, float track_row_margin) {
  //
  auto* ui_track_sys = &context.ui_track_system;

  const auto tracks = track::read_tracks(ui_track_sys);
  const int num_tracks = int(tracks.size());

  auto* layout = data.layout;
  layout::begin_group(layout, cont, layout::GroupOrientation::Row, 0, 0, layout::JustifyContent::None);
  DynamicArray<int, 64> track_rows;
  for (int i = 0; i < num_tracks; i++) {
    const int tr = layout::box(layout, {1}, {1, track_row_height, track_row_height});
    layout::set_box_cursor_events(layout, tr, {layout::BoxCursorEvents::Click});
    layout::set_box_margin(layout, tr, 0, i == 0 ? track_row_margin : 0, 0, track_row_margin);
    track_rows.push_back(tr);

    auto& track = tracks[i];
    elements::push_stateful_button(
      &data.gui_elements, tr, elements::StatefulButtonData::from_uint32(track.handle.id), select_track);
  }
  //  +1 for new track
  track_rows.push_back(layout::box(layout, {1}, {1, track_row_height, track_row_height}));
  if (num_tracks == 0) {
    layout::set_box_margin(layout, track_rows.back(), 0, track_row_margin, 0, track_row_margin);
  }
  layout::end_group(layout);
  for (int i = 0; i < track_rows.size(); i++) {
    auto color = Vec3f{1.0f, 1.0f, 1.0f};
    if (i < num_tracks && is_selected(data, tracks[i].handle)) {
      color *= 0.5f;
    }
    auto style = ui::make_render_quad_desc_style(color, 0, {}, 0, 0.5f);
    gui::draw_box(data.draw_list, layout, track_rows[i], style);
  }

  for (int i = 0; i < num_tracks; i++) {
    prepare_track_header(data, context, tracks[i], track_rows[i]);
  }

  { //  new track
    assert(!track_rows.empty());
    layout::set_box_is_clickable(layout, track_rows.back());
    elements::push_button(&data.gui_elements, track_rows.back(), create_track);
//    layout::begin_group(layout, track_rows.back(), layout::GroupOrientation::Col);
//    int box = layout::box(layout, {1, 32, 32}, {1, 32, 32});
//    layout::set_box_cursor_events(layout, box, {layout::BoxCursorEvents::Click});
//    layout::end_group(layout);
//
//    gui::draw_box(data.draw_list, layout, box, ui::make_render_quad_desc_style(Vec3f{1.0f}, 2.0f));
//    elements::push_button(&data.gui_elements, box, create_track);
  }
}

} //  anon

void ui::prepare_audio_track_editor(const AudioEditorCommonContext& context) {
  auto& data = globals.data;
  data.prepared = false;

  if (!data.layout) {
    data.layout = gui::layout::create_layout(GUI_LAYOUT_ID); //  @TODO
  }

  auto* layout = data.layout;
  layout::clear_layout(layout);
  data.draw_list.clear();

  if (context.hidden || context.mode != AudioEditorMode::Track) {
    return;
  }

  elements::begin_elements(&data.gui_elements, GUI_LAYOUT_ID);

  auto* ui_track_sys = &context.ui_track_system;

  if (data.selected_track) {
    auto* track = track::read_track(ui_track_sys, data.selected_track.value());
    if (!track) {
      data.selected_track = NullOpt{};
    }
  }

  const auto tracks = track::read_tracks(ui_track_sys);
  const int num_tracks = int(tracks.size());

  auto fb_dims = context.container_dimensions;
  layout::set_root_dimensions(layout, fb_dims.x, fb_dims.y);

  const float root_w = 768.0f;
  const float root_h = 512.0f;

  layout::begin_group(layout, 0, layout::GroupOrientation::Col, 0, 0);
  int root = layout::box(layout, {1, root_w, root_w}, {1, root_h, root_h});
  layout::end_group(layout);

  const float header_height = 24.0f;
  const float track_body_height = 256.0f + 96.0f;
  const float track_footer_height = root_h - (header_height + track_body_height);

  layout::begin_group(layout, root, layout::GroupOrientation::Row, 0, 0, layout::JustifyContent::None);
  const int header = layout::box(layout, {1}, {1, header_height, header_height});
  const int body = layout::box(layout, {1}, {1, track_body_height, track_body_height});
  const int footer = layout::box(layout, {1}, {1, track_footer_height, track_footer_height});
  layout::end_group(layout);

  prepare_header(data, header, context);

//  gui::draw_box(data.draw_list, data.layout, root, {});
//  gui::draw_box(data.draw_list, layout, header, ui::make_render_quad_desc_style(Vec3f{1.0f}));
//  gui::draw_box(data.draw_list, layout, body, ui::make_render_quad_desc_style(Vec3f{1.0f, 0.0f, 0.0f}));
//  gui::draw_box(data.draw_list, layout, footer, ui::make_render_quad_desc_style(Vec3f{1.0f, 0.0f, 1.0f}));

  const float track_row_height = 48.0f;
  const float track_row_margin = 16.0f;
  const float track_tot_height = track_row_height * std::max(16.0f, float(num_tracks + 1));

  layout::begin_group(layout, body, layout::GroupOrientation::Row, 0, 0, layout::JustifyContent::None);
  const int track_cont = layout::box(layout, {1}, {1, track_tot_height, track_tot_height});
  layout::set_box_cursor_events(layout, track_cont, {layout::BoxCursorEvents::Scroll});
  layout::end_group(layout);

  float track_v_scroll{};
  cursor::read_scroll_offsets(&context.cursor_state, BOXIDI(track_cont), nullptr, &track_v_scroll);

  const float track_frac_width = 0.25f;
  layout::begin_group(layout, track_cont, layout::GroupOrientation::Col, 0, track_v_scroll);
  const int track_col0 = layout::box(layout, {track_frac_width}, {1});
  const int track_col1 = layout::box(layout, {1.0f - track_frac_width}, {1});
  layout::set_box_cursor_events(layout, track_col1, {layout::BoxCursorEvents::Scroll});
  layout::end_group(layout);

  float track_grid_h_scroll{};
  cursor::read_scroll_offsets(&context.cursor_state, BOXIDI(track_col1), &track_grid_h_scroll, nullptr);

  layout::begin_group(layout, track_col1, layout::GroupOrientation::Col, track_grid_h_scroll, 0);
  const int clip_cont = layout::box(layout, {1}, {1});
  layout::end_group(layout);

//  gui::draw_box(data.draw_list, layout, track_col0, ui::make_render_quad_desc_style(Vec3f{1.0f, 1.0f, 0.0f}));
//  gui::draw_box(data.draw_list, layout, clip_cont, ui::make_render_quad_desc_style(Vec3f{0.0f, 1.0f, 1.0f}));

  prepare_clips(data, clip_cont, context, track_row_height, track_row_margin);
  prepare_tracks(data, track_col0, context, track_row_height, track_row_margin);

  layout::begin_group(layout, footer, layout::GroupOrientation::Row);
  int footer_row0 = layout::box(layout, {1}, {1, header_height, header_height});
  int footer_row1 = layout::box(layout, {1}, {1, track_footer_height - header_height, track_footer_height - header_height});
  layout::end_group(layout);

//  gui::draw_box(data.draw_list, layout, footer_row0, ui::make_render_quad_desc_style(Vec3f{1.0f}, 2.0f));
//  gui::draw_box(data.draw_list, layout, footer_row1, ui::make_render_quad_desc_style(Vec3f{1.0f}, 2.0f));

  layout::begin_group(layout, footer_row0, layout::GroupOrientation::Col);
  layout::box(layout, {track_frac_width}, {1});
  const int footer_track_col1 = layout::box(layout, {1.0f - track_frac_width}, {1});
  layout::end_group(layout);

  if (data.selected_track) {
    prepare_clip_length_slider(data, footer_track_col1, context);
  }

  //  footer1
  layout::begin_group(layout, footer_row1, layout::GroupOrientation::Col);
  const int footer_clip_col0 = layout::box(layout, {track_frac_width}, {1});
  const int footer_clip_col1 = layout::box(layout, {1.0f - track_frac_width}, {1});
  layout::end_group(layout);

  if (data.selected_track) {
    const float cont_h = layout::read_box(layout, footer_clip_col0).content_height();

    layout::begin_group(layout, footer_clip_col0, layout::GroupOrientation::Row);
    int mode_select_cont = layout::box(layout, {1}, {1, header_height, header_height});
    int rest_cont = layout::box(layout, {1}, {1, cont_h - header_height, cont_h - header_height});
    layout::end_group(layout);

    prepare_track_mode_selector(data, mode_select_cont, context);

    if (data.track_control_mode == TrackControlMode::NoteClipStateMachine) {
      prepare_ncsm_control(data, rest_cont, context);
    } else if (data.track_control_mode == TrackControlMode::Arp) {
      prepare_arp_control(data, rest_cont, context);
    }

    layout::set_box_is_scrollable(layout, footer_clip_col1);
    prepare_clip(data, footer_clip_col1, context);
  }

  //  end
  cursor::evaluate_boxes(&context.cursor_state, layout);

  data.prepared = true;
}

void ui::evaluate_audio_track_editor(const AudioEditorCommonContext& context) {
  auto& data = globals.data;
  if (!data.prepared) {
    return;
  }
  elements::evaluate(&data.gui_elements, &context.cursor_state, (void*) &context);
  elements::end_elements(&data.gui_elements);
}

void ui::render_audio_track_editor(const AudioEditorCommonContext& context) {
  auto& data = globals.data;
  if (!data.prepared) {
    return;
  }
  gui::modify_style_from_cursor_events(data.draw_list, &context.cursor_state, 0.75f);
  gui::set_box_quad_positions(data.draw_list, data.layout);
  gui::modify_box_quad_positions_from_cursor_events(data.draw_list, &context.cursor_state, 0.75f);
  gui::push_draw_list(&context.render_data, data.draw_list);
}

void ui::destroy_audio_track_editor() {
  auto& data = globals.data;
  layout::destroy_layout(&data.layout);
}

GROVE_NAMESPACE_END

#undef GUI_LAYOUT_ID
#undef BOXIDI