#include "audio_timeline_editor.hpp"
#include "ui_common.hpp"
#include "../audio_core/pitch_sampling.hpp"
#include "../audio_core/rhythm_parameters.hpp"
#include "../render/render_gui_data.hpp"
#include "../audio_core/AudioComponent.hpp"
#include "../audio_core/audio_port_placement.hpp"
#include "../audio_core/audio_node_attributes.hpp"
#include "grove/gui/gui_layout.hpp"
#include "grove/gui/gui_cursor.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Optional.hpp"
#include "grove/input/KeyTrigger.hpp"
#include "grove/input/MouseButtonTrigger.hpp"

GROVE_NAMESPACE_BEGIN

#define BOXIDI(i) grove::gui::layout::BoxID::create(2, (i))

namespace {

using namespace gui;

struct PendingBox;
struct AudioTimelineEditorData;

using CommonContext = ui::AudioEditorCommonContext;
using CursorCallback = void(PendingBox&, AudioTimelineEditorData&, const CommonContext&);
using ViewNoteClipTrackNodes = ArrayView<const UITimelineSystem::NoteClipTrackNode>;

struct Config {
  static constexpr float min_bpm = 20.0f;
  static constexpr float max_bpm = 240.0f;
  static constexpr float min_ref_st = 40.0f;
  static constexpr float max_ref_st = 80.0f;
//  static constexpr float min_ref_freq = 300.0f;
//  static constexpr float max_ref_freq = 580.0f;
};

struct PendingBox {
  layout::BoxID box_id{};
  AudioNodeStorage::PortID port_id{};
  bool connected{};
  bool is_track_selector{};
  Vec3f color{};
  Optional<RenderQuadDescriptor> quad_desc;
  NoteClipHandle note_clip{};
  TimelineNoteClipTrackHandle note_clip_track{};
  Optional<ClipNote> clip_note;
  CursorCallback* left_click_callback{};
  CursorCallback* left_down_callback{};
};

struct TrackView {
  TimelineNoteClipTrackHandle track{};
  ScoreRegion region{{}, ScoreCursor{16, 0.0}};
  float last_box_width{};
};

struct ClipView {
  TimelineNoteClipTrackHandle track{};
  NoteClipHandle clip{};
  ScoreCursor span_size{4, 0.0};
  Vec2f semitone_span{-12.0f, 24.0f};
};

struct DraggingFloat {
  float x0{};
  float value0{};
  float candidate_value{};
  float container_width{};
};

struct DraggingLoop {
  float x0{};
  TimelineNoteClipTrackHandle track{};
  ScoreRegion candidate_region{};
};

struct DraggingClip {
  float x0{};
  TimelineNoteClipTrackHandle track{};
  NoteClipHandle clip{};
  ScoreRegion candidate_region{};
};

struct SelectedNoteClip {
  TimelineNoteClipTrackHandle track;
  NoteClipHandle clip;
};

struct AudioTimelineEditorData {
  layout::Layout* layout{};
  std::vector<PendingBox> pending;
  std::vector<TrackView> track_views;
  DynamicArray<TimelineNoteClipTrackHandle, 4> selected_note_clip_tracks;
  Optional<ClipView> active_clip_view;
  std::vector<ClipNote> selected_clip_notes;
  std::vector<SelectedNoteClip> selected_note_clips;
  Optional<DraggingLoop> dragging_loop_size;
  Optional<DraggingLoop> dragging_loop_offset;
  Optional<DraggingClip> dragging_clip_offset;
  Optional<DraggingClip> dragging_clip_size;
  Optional<DraggingFloat> dragging_bpm;
  Optional<DraggingFloat> dragging_reference_semitone;
  Optional<DraggingFloat> dragging_global_p_quantized;
  Optional<DraggingFloat> dragging_note_sets[2];
  Optional<SelectedNoteClip> copied_clip;
};

double as_beats(const ScoreCursor& curs) {
  return curs.to_beats(reference_time_signature().numerator);
}

TrackView* find_track_view(AudioTimelineEditorData& data, TimelineNoteClipTrackHandle handle) {
  auto it = std::find_if(data.track_views.begin(), data.track_views.end(), [handle](auto& view) {
    return view.track == handle;
  });
  return it == data.track_views.end() ? nullptr : &*it;
}

const ClipNote* find_selected_note(const AudioTimelineEditorData& data, ClipNote note) {
  for (auto& sel : data.selected_clip_notes) {
    if (sel == note) {
      return &sel;
    }
  }
  return nullptr;
}

const NoteClipHandle* find_selected_clip(const AudioTimelineEditorData& data, NoteClipHandle handle) {
  for (auto& sel : data.selected_note_clips) {
    if (sel.clip == handle) {
      return &sel.clip;
    }
  }
  return nullptr;
}

void find_track_views(AudioTimelineEditorData& data, const ViewNoteClipTrackNodes& nodes, int* dst) {
  { //  remove expired
    auto it = data.track_views.begin();
    while (it != data.track_views.end()) {
      auto handle = it->track;
      auto track_it = std::find_if(nodes.begin(), nodes.end(), [handle](auto& track) {
        return track.track_handle == handle;
      });

      if (track_it == nodes.end()) {
        //  no longer exists, so erase it and the selection, if present.
        auto sel_it = std::find(
          data.selected_note_clip_tracks.begin(), data.selected_note_clip_tracks.end(), handle);
        if (sel_it != data.selected_note_clip_tracks.end()) {
          data.selected_note_clip_tracks.erase(sel_it);
        }
        it = data.track_views.erase(it);
      } else {
        ++it;
      }
    }
  }

  //  add new
  auto& views = data.track_views;
  for (int i = 0; i < int(nodes.size()); i++) {
    auto& node = nodes[i];
    auto* it = find_track_view(data, node.track_handle);
    if (!it) {
      dst[i] = int(views.size());
      auto& view = views.emplace_back();
      view.track = node.track_handle;
    } else {
      dst[i] = int(it - views.data());
    }
  }
}

bool has_selected_track(const AudioTimelineEditorData& data, const PendingBox& box) {
  if (box.is_track_selector) {
    for (TimelineNoteClipTrackHandle track : data.selected_note_clip_tracks) {
      if (track == box.note_clip_track) {
        return true;
      }
    }
  }
  return false;
}

bool has_selected_note(const AudioTimelineEditorData& data, const PendingBox& box) {
  if (box.clip_note) {
    return find_selected_note(data, box.clip_note.value()) != nullptr;
  } else {
    return false;
  }
}

bool has_selected_clip(const AudioTimelineEditorData& data, const PendingBox& box) {
  if (box.note_clip.is_valid()) {
    return find_selected_clip(data, box.note_clip) != nullptr;
  } else {
    return false;
  }
}

DraggingLoop get_dragging_loop(PendingBox& box, const CommonContext& context) {
  auto* track = ui_read_note_clip_track(
    &context.audio_component.timeline_system, box.note_clip_track);
  assert(track && track->loop_region);
  const auto coords = context.mouse_button_trigger.get_coordinates();
  DraggingLoop drag{};
  drag.track = box.note_clip_track;
  drag.x0 = float(coords.first);
  drag.candidate_region = track->loop_region.unwrap();
  return drag;
}

DraggingClip get_dragging_clip(PendingBox& box, const CommonContext& context) {
  auto* clip = ui_read_clip(
    context.audio_component.timeline_system.clip_system, box.note_clip);
  assert(clip);
  const auto coords = context.mouse_button_trigger.get_coordinates();
  DraggingClip drag{};
  drag.track = box.note_clip_track;
  drag.clip = box.note_clip;
  drag.x0 = float(coords.first);
  drag.candidate_region = clip->span;
  return drag;
}

void begin_drag_bpm(
  PendingBox& box, AudioTimelineEditorData& data, const CommonContext& context) {
  //
  DraggingFloat drag{};
  drag.x0 = float(context.mouse_button_trigger.get_coordinates().first);
  drag.candidate_value = float(context.audio_component.audio_transport.get_bpm());
  drag.value0 = drag.candidate_value;
  drag.container_width = box.color.x; //  @NOTE
  data.dragging_bpm = drag;
}

void begin_drag_global_p_quantized(
  PendingBox& box, AudioTimelineEditorData& data, const CommonContext& context) {
  //
  DraggingFloat drag{};
  drag.x0 = float(context.mouse_button_trigger.get_coordinates().first);
  drag.candidate_value = context.rhythm_parameters.global_p_quantized;
  drag.value0 = drag.candidate_value;
  drag.container_width = box.color.x; //  @NOTE
  data.dragging_global_p_quantized = drag;
}

void begin_drag_reference_semitone(
  PendingBox& box, AudioTimelineEditorData& data, const CommonContext& context) {
  //
  DraggingFloat drag{};
  drag.x0 = float(context.mouse_button_trigger.get_coordinates().first);
  drag.candidate_value = float(context.audio_component.get_ui_scale()->get_tuning()->reference_semitone);
  drag.value0 = drag.candidate_value;
  drag.container_width = box.color.x; //  @NOTE
  data.dragging_reference_semitone = drag;
}

void begin_drag_primary_note_set_index(
  PendingBox& box, AudioTimelineEditorData& data, const CommonContext& context) {
  //
  DraggingFloat drag{};
  drag.x0 = float(context.mouse_button_trigger.get_coordinates().first);
  drag.candidate_value = float(context.pitch_sampling_parameters.primary_note_set_index);
  drag.value0 = drag.candidate_value;
  drag.container_width = box.color.x; //  @NOTE
  data.dragging_note_sets[0] = drag;
}

void begin_drag_secondary_note_set_index(
  PendingBox& box, AudioTimelineEditorData& data, const CommonContext& context) {
  //
  DraggingFloat drag{};
  drag.x0 = float(context.mouse_button_trigger.get_coordinates().first);
  drag.candidate_value = float(context.pitch_sampling_parameters.secondary_note_set_index);
  drag.value0 = drag.candidate_value;
  drag.container_width = box.color.x; //  @NOTE
  data.dragging_note_sets[1] = drag;
}

void begin_drag_clip_offset(
  PendingBox& box, AudioTimelineEditorData& data, const CommonContext& context) {
  //
  data.dragging_clip_offset = get_dragging_clip(box, context);
}

void begin_drag_clip_size(
  PendingBox& box, AudioTimelineEditorData& data, const CommonContext& context) {
  //
  data.dragging_clip_size = get_dragging_clip(box, context);
}

void begin_drag_loop_size(
  PendingBox& box, AudioTimelineEditorData& data, const CommonContext& context) {
  //
  data.dragging_loop_size = get_dragging_loop(box, context);
}

void begin_drag_loop_offset(
  PendingBox& box, AudioTimelineEditorData& data, const CommonContext& context) {
  //
  data.dragging_loop_offset = get_dragging_loop(box, context);
}

void create_note_clip_track(PendingBox&, AudioTimelineEditorData&, const CommonContext& context) {
  auto pss_group = context.pitch_sampling_parameters.get_primary_group_handle(
    context.audio_component.get_pitch_sampling_system());
  context.audio_component.get_ui_timeline_system()->create_note_clip_track(
    *context.audio_component.get_timeline_system(),
    *context.audio_component.get_arpeggiator_system(),
    context.audio_component.audio_node_storage, pss_group);
}

void select_track(PendingBox& box, AudioTimelineEditorData& data, const CommonContext&) {
  for (auto& track : data.selected_note_clip_tracks) {
    if (track == box.note_clip_track) {
      return;
    }
  }
  data.selected_note_clip_tracks.push_back(box.note_clip_track);
}

void select_note(PendingBox& box, AudioTimelineEditorData& data, const CommonContext&) {
  assert(box.clip_note);
  if (!find_selected_note(data, box.clip_note.value())) {
    data.selected_clip_notes.push_back(box.clip_note.value());
  }
}

void select_port(PendingBox& box, AudioTimelineEditorData&, const CommonContext& context) {
  context.selected.insert(box.port_id);
}

void select_clip(PendingBox& box, AudioTimelineEditorData& data, const CommonContext&) {
  if (!find_selected_clip(data, box.note_clip)) {
    SelectedNoteClip sel{};
    sel.clip = box.note_clip;
    sel.track = box.note_clip_track;
    data.selected_note_clips.push_back(sel);
  }
}

void activate_clip(PendingBox& box, AudioTimelineEditorData& data, const CommonContext&) {
  if (data.active_clip_view && data.active_clip_view.value().clip == box.note_clip) {
    return;
  }

  ClipView active_view{};
  active_view.track = box.note_clip_track;
  active_view.clip = box.note_clip;
  data.active_clip_view = active_view;
}

void select_and_activate_clip(
  PendingBox& box, AudioTimelineEditorData& data, const CommonContext& context) {
  activate_clip(box, data, context);
  select_clip(box, data, context);
}

void toggle_metronome(PendingBox&, AudioTimelineEditorData&, const CommonContext& context) {
  metronome::ui_toggle_enabled(context.audio_component.get_metronome());
}

void toggle_midi_output_enabled(
  PendingBox& box, AudioTimelineEditorData&, const CommonContext& context) {
  //
  auto& comp = context.audio_component;
  comp.ui_timeline_system.toggle_midi_output_enabled(
    *comp.get_midi_message_stream_system(),
    *comp.get_triggered_notes(), box.note_clip_track);
}

void toggle_midi_recording(PendingBox& box, AudioTimelineEditorData&, const CommonContext& context) {
  context.audio_component.ui_timeline_system.toggle_recording_enabled(box.note_clip_track);
}

void toggle_arp_enabled(PendingBox& box, AudioTimelineEditorData&, const CommonContext& context) {
  context.audio_component.ui_timeline_system.toggle_arp_enabled(
    *context.audio_component.get_timeline_system(),
    *context.audio_component.get_arpeggiator_system(),
    box.note_clip_track);
}

void create_note_clip(PendingBox& box, AudioTimelineEditorData&, const CommonContext& context) {
  auto& tsys = context.audio_component.timeline_system;
  auto end = ui_get_track_span_end(&tsys, box.note_clip_track);
  auto size = ScoreCursor{1, 0.0};
  ui_create_timeline_note_clip(&tsys, box.note_clip_track, ScoreRegion{end, size});
}

void update_selected_clips(AudioTimelineEditorData& data, const CommonContext& context) {
  auto it = data.selected_note_clips.begin();
  while (it != data.selected_note_clips.end()) {
    if (!ui_is_clip(context.audio_component.timeline_system.clip_system, it->clip)) {
      it = data.selected_note_clips.erase(it);
    } else {
      ++it;
    }
  }
}

void update_dragging_float(DraggingFloat& drag, float min_v, float max_v, const CommonContext& context,
                           const Optional<float>& floor_factor) {
  const auto x = float(context.mouse_button_trigger.get_coordinates().first);
  float dx = x - drag.x0;
  float bpm_span = max_v - min_v;
  float frac_val = clamp(dx / drag.container_width, -1.0f, 1.0f) * bpm_span;
  float new_val = clamp(frac_val + drag.value0, min_v, max_v);
  if (floor_factor) {
    new_val = std::floor(new_val * floor_factor.value()) / floor_factor.value();
  }
  drag.candidate_value = new_val;
}

void update_dragging_bpm(AudioTimelineEditorData& data, const CommonContext& context) {
  if (data.dragging_bpm) {
    auto& drag = data.dragging_bpm.value();
    update_dragging_float(drag, Config::min_bpm, Config::max_bpm, context, Optional<float>(2.0f));
  }
}

void update_dragging_global_p_quantized(AudioTimelineEditorData& data, const CommonContext& context) {
  if (data.dragging_global_p_quantized) {
    auto& drag = data.dragging_global_p_quantized.value();
    update_dragging_float(drag, 0.0f, 1.0f, context, NullOpt{});
  }
}

void update_dragging_reference_semitone(AudioTimelineEditorData& data, const CommonContext& context) {
  if (data.dragging_reference_semitone) {
    auto& drag = data.dragging_reference_semitone.value();
    update_dragging_float(drag, Config::min_ref_st, Config::max_ref_st, context, Optional<float>(1.0f));
  }
}

void update_dragging_note_sets(AudioTimelineEditorData& data, const CommonContext& context) {
  for (int i = 0; i < 2; i++) {
    if (data.dragging_note_sets[i]) {
      auto& drag = data.dragging_note_sets[i].value();
      update_dragging_float(
        drag,
        float(pss::PitchSamplingParameters::min_note_set_index()),
        float(pss::PitchSamplingParameters::max_note_set_index()), context, NullOpt{});
    }
  }
}

void update_dragging_clip_size(AudioTimelineEditorData& data, const CommonContext& context) {
  if (!data.dragging_clip_size) {
    return;
  }

  auto& drag = data.dragging_clip_size.value();
  auto& tsys = context.audio_component.timeline_system;
  const auto* track = ui_read_note_clip_track(&tsys, drag.track);
  if (!track) {
    data.dragging_clip_size = NullOpt{};
    return;
  }

  const auto* clip = ui_read_clip(tsys.clip_system, drag.clip);
  if (!clip) {
    data.dragging_clip_size = NullOpt{};
    return;
  }

  const auto* track_view = find_track_view(data, drag.track);
  assert(track_view);

  const auto x = float(context.mouse_button_trigger.get_coordinates().first);
  float dx = x - drag.x0;

  assert(track_view->last_box_width > 0.0f);
  auto view_size = as_beats(track_view->region.size);
  auto dbeats = view_size * std::max(-1.0f, std::min(1.0f, dx / track_view->last_box_width));
  auto quant = audio::Quantization::Quarter;
  dbeats = audio::quantize_floor(dbeats, quant, reference_time_signature().numerator);

  auto span_size = as_beats(clip->span.size);
  auto new_span_size = std::max(1.0, span_size + dbeats);
  drag.candidate_region.size = ScoreCursor::from_beats(
    new_span_size, reference_time_signature().numerator);
}

void update_dragging_clip_offset(AudioTimelineEditorData& data, const CommonContext& context) {
  if (!data.dragging_clip_offset) {
    return;
  }

  auto& drag = data.dragging_clip_offset.value();
  auto& tsys = context.audio_component.timeline_system;
  const auto* track = ui_read_note_clip_track(&tsys, drag.track);
  if (!track) {
    data.dragging_clip_offset = NullOpt{};
    return;
  }

  const auto* clip = ui_read_clip(tsys.clip_system, drag.clip);
  if (!clip) {
    data.dragging_clip_offset = NullOpt{};
    return;
  }

  const auto* track_view = find_track_view(data, drag.track);
  assert(track_view);

  const auto x = float(context.mouse_button_trigger.get_coordinates().first);
  float dx = x - drag.x0;

  assert(track_view->last_box_width > 0.0f);
  auto view_size = as_beats(track_view->region.size);
  auto dbeats = view_size * std::max(-1.0f, std::min(1.0f, dx / track_view->last_box_width));
  auto quant = audio::Quantization::Quarter;
  dbeats = audio::quantize_floor(dbeats, quant, reference_time_signature().numerator);

  auto span_beg = as_beats(clip->span.begin);
  auto new_span_beg = std::max(0.0, span_beg + dbeats);
  drag.candidate_region.begin = ScoreCursor::from_beats(
    new_span_beg, reference_time_signature().numerator);
}

void update_dragging_loop_offset(AudioTimelineEditorData& data, const CommonContext& context) {
  if (!data.dragging_loop_offset) {
    return;
  }

  auto& drag = data.dragging_loop_offset.value();

  auto& tsys = context.audio_component.timeline_system;
  auto* track = ui_read_note_clip_track(&tsys, drag.track);
  if (!track || !track->loop_region) {
    data.dragging_loop_offset = NullOpt{};
    return;
  }

  const auto& loop_reg = track->loop_region.value();
  auto* track_view = find_track_view(data, drag.track);
  assert(track_view);

  const auto x = float(context.mouse_button_trigger.get_coordinates().first);
  float dx = x - drag.x0;

  assert(track_view->last_box_width > 0.0f);
  auto view_size = as_beats(track_view->region.size);
  auto dbeats = view_size * std::max(-1.0f, std::min(1.0f, dx / track_view->last_box_width));
  auto quant = audio::Quantization::Quarter;
  dbeats = audio::quantize_floor(dbeats, quant, reference_time_signature().numerator);

  auto loop_beg = as_beats(loop_reg.begin);
  auto new_loop_beg = std::max(0.0, loop_beg + dbeats);
  drag.candidate_region.begin = ScoreCursor::from_beats(
    new_loop_beg, reference_time_signature().numerator);
}

void update_dragging_loop_size(AudioTimelineEditorData& data, const CommonContext& context) {
  if (!data.dragging_loop_size) {
    return;
  }

  auto& drag = data.dragging_loop_size.value();

  auto& tsys = context.audio_component.timeline_system;
  auto* track = ui_read_note_clip_track(&tsys, drag.track);
  if (!track || !track->loop_region) {
    data.dragging_loop_size = NullOpt{};
    return;
  }

  const auto& loop_reg = track->loop_region.value();
  auto* track_view = find_track_view(data, drag.track);
  assert(track_view);

  const auto x = float(context.mouse_button_trigger.get_coordinates().first);
  float dx = x - drag.x0;

  assert(track_view->last_box_width > 0.0f);
  auto view_size = as_beats(track_view->region.size);
  auto dbeats = view_size * std::max(-1.0f, std::min(1.0f, dx / track_view->last_box_width));
  auto quant = audio::Quantization::Quarter;
  dbeats = audio::quantize_floor(dbeats, quant, reference_time_signature().numerator);

  auto loop_size = as_beats(loop_reg.size);
  auto new_loop_size = std::max(1.0, loop_size + dbeats);
  drag.candidate_region.size = ScoreCursor::from_beats(
    new_loop_size, reference_time_signature().numerator);
}

void prepare(AudioTimelineEditorData& data, const CommonContext& context) {
  if (!data.layout) {
    data.layout = gui::layout::create_layout(2);
  }

  auto* layout = data.layout;
  layout::clear_layout(layout);
  data.pending.clear();

  auto* cursor_state = &context.cursor_state;
  const auto* audio_component = &context.audio_component;
  auto& ui_timeline_system = audio_component->ui_timeline_system;
  const auto note_clip_tracks = ui_timeline_system.read_note_clip_track_nodes();

  if (context.hidden || context.mode != ui::AudioEditorMode::Timeline) {
    return;
  }

  if (context.mouse_button_trigger.newly_pressed(Mouse::Button::Left) &&
      !context.key_trigger.is_pressed(Key::LeftControl)) {
    data.selected_clip_notes.clear();
    data.selected_note_clips.clear();
    data.selected_note_clip_tracks.clear();
  }

  if (context.key_trigger.newly_pressed(Key::UpArrow) ||
      context.key_trigger.newly_pressed(Key::DownArrow)) {
    bool zoom_in = context.key_trigger.newly_pressed(Key::UpArrow);
    for (auto& view : data.track_views) {
      view.region.size.wrapped_scale(zoom_in ? 0.5 : 2.0, reference_time_signature().numerator);
    }
  }

  if (data.copied_clip) {
    if (!ui_is_clip(audio_component->timeline_system.clip_system, data.copied_clip.value().clip)) {
      data.copied_clip = NullOpt{};
    }
  }

  update_selected_clips(data, context);
  update_dragging_loop_size(data, context);
  update_dragging_loop_offset(data, context);
  update_dragging_clip_offset(data, context);
  update_dragging_clip_size(data, context);
  update_dragging_bpm(data, context);
  update_dragging_global_p_quantized(data, context);
  update_dragging_reference_semitone(data, context);
  update_dragging_note_sets(data, context);

  const NoteClip* active_note_clip{};
  if (data.active_clip_view) {
    auto& act_view = data.active_clip_view.value();
    auto* clip = ui_read_clip(audio_component->timeline_system.clip_system, act_view.clip);
    if (clip) {
      active_note_clip = clip;
      act_view.span_size = clip->span.size;
      if (data.dragging_clip_size && data.dragging_clip_size.value().clip == act_view.clip) {
        act_view.span_size = data.dragging_clip_size.value().candidate_region.size;
      }
    } else {
      data.active_clip_view = NullOpt{};
    }
  }

  Temporary<int, 1024> store_track_view_indices;
  int* track_view_indices = store_track_view_indices.require(int(note_clip_tracks.size()));
  find_track_views(data, note_clip_tracks, track_view_indices);

  auto fb_dims = context.container_dimensions;
  layout::set_root_dimensions(layout, fb_dims.x, fb_dims.y);

  layout::begin_group(layout, 0, layout::GroupOrientation::Col, 0, 0);
  int root = layout::box(layout, {1, 768, 768}, {1, 512, 512});
  layout::end_group(layout);

#if 0
  {
    auto box = layout::read_box(layout, root);
    if (!box.is_clipped()) {
      auto& pend = data.pending.emplace_back();
      pend.box_id = BOXIDI(root);
      pend.quad_desc = ui::make_render_quad_desc(box, Vec3f{1.0f});
    }
  }
#endif

  layout::begin_group(layout, root, layout::GroupOrientation::Row);
  const int panel0 = layout::box(layout, {1}, {0.125f * 0.5f});
  const int top_panel = layout::box(layout, {1}, {0.5f - 0.125f * 0.5f});
  layout::set_box_cursor_events(layout, top_panel, {layout::BoxCursorEvents::Scroll});
  const int bot_panel = layout::box(layout, {1}, {0.5f});
  layout::end_group(layout);

  {
    layout::begin_group(layout, panel0, layout::GroupOrientation::Col);
    int metronome_toggle_cont = layout::box(layout, {0.125f}, {1});
    int bpm_slider_cont = layout::box(layout, {0.25f}, {1});
    int p_quantized_cont = layout::box(layout, {0.125f}, {1});
    int st_cont = layout::box(layout, {0.125f}, {1});
    int note_set_conts[2];
    note_set_conts[0] = layout::box(layout, {0.125f}, {1});
    note_set_conts[1] = layout::box(layout, {0.125f}, {1});
    layout::end_group(layout);

    {
      auto& pend = data.pending.emplace_back();
      pend.box_id = BOXIDI(bpm_slider_cont);
      pend.quad_desc = ui::make_render_quad_desc(layout::read_box(layout, bpm_slider_cont), Vec3f{1.0f, 0.0f, 0.0f});
    }
    {
      auto& pend = data.pending.emplace_back();
      pend.box_id = BOXIDI(p_quantized_cont);
      pend.quad_desc = ui::make_render_quad_desc(layout::read_box(layout, p_quantized_cont), Vec3f{1.0f, 0.0f, 1.0f});
    }
    {
      auto& pend = data.pending.emplace_back();
      pend.box_id = BOXIDI(st_cont);
      pend.quad_desc = ui::make_render_quad_desc(layout::read_box(layout, st_cont), Vec3f{1.0f, 1.0f, 0.0f});
    }
    for (int nsi = 0; nsi < 2; nsi++) {
      auto& pend = data.pending.emplace_back();
      pend.box_id = BOXIDI(note_set_conts[nsi]);
      auto color = nsi == 0 ? Vec3f{0.0f, 0.0f, 1.0f} : Vec3f{0.0f, 1.0f, 1.0f};
      pend.quad_desc = ui::make_render_quad_desc(layout::read_box(layout, note_set_conts[nsi]), color);
    }
    { //  metronome toggle
      layout::begin_group(layout, metronome_toggle_cont, layout::GroupOrientation::Col);
      int toggle = layout::box(layout, {1, 32, 32}, {1, 32, 32});
      layout::set_box_cursor_events(layout, toggle, {layout::BoxCursorEvents::Click});
      layout::end_group(layout);

      auto& pend = data.pending.emplace_back();
      auto color = metronome::ui_is_enabled(context.audio_component.get_metronome()) ? Vec3f{0.25f} : Vec3f{0.5f};
      pend.box_id = BOXIDI(toggle);
      pend.quad_desc = ui::make_render_quad_desc(layout::read_box(layout, toggle), color);
      pend.quad_desc.value().border_px = 2.0f;
      pend.quad_desc.value().linear_border_color = Vec3f{0.75f};
      pend.left_click_callback = toggle_metronome;
    }
    { //  bpm
      auto curr_bpm = float(context.audio_component.audio_transport.get_bpm());
      if (data.dragging_bpm) {
        curr_bpm = data.dragging_bpm.value().candidate_value;
      }

      const float frac_bpm = clamp01((curr_bpm - Config::min_bpm) / (Config::max_bpm - Config::min_bpm));
      const auto bpm_box = layout::read_box(layout, bpm_slider_cont);

      const float handle_w = bpm_box.content_height();
      const float px_span = bpm_box.content_width() - handle_w;
      const float xoff = px_span * frac_bpm;

      layout::begin_group(layout, bpm_slider_cont, layout::GroupOrientation::Manual, 0, 0, layout::JustifyContent::None);
      int handle = layout::box(layout, {1, handle_w, handle_w}, {1});
      layout::set_box_cursor_events(layout, handle, {layout::BoxCursorEvents::Click});
      layout::set_box_offsets(layout, handle, xoff, 0);
      layout::end_group(layout);

      auto& pend = data.pending.emplace_back();
      pend.box_id = BOXIDI(handle);
      pend.quad_desc = ui::make_render_quad_desc(layout::read_box(layout, handle), Vec3f{0.5f});
      pend.left_down_callback = begin_drag_bpm;
      pend.color.x = px_span;
    }
    { //  p_quantized
      float curr_p = context.rhythm_parameters.global_p_quantized;
      if (data.dragging_global_p_quantized) {
        curr_p = data.dragging_global_p_quantized.value().candidate_value;
      }

      const auto p_box = layout::read_box(layout, p_quantized_cont);
      const float handle_w = p_box.content_height();
      const float px_span = p_box.content_width() - handle_w;
      const float xoff = px_span * curr_p;

      layout::begin_group(layout, p_quantized_cont, layout::GroupOrientation::Manual, 0, 0, layout::JustifyContent::None);
      int handle = layout::box(layout, {1, handle_w, handle_w}, {1});
      layout::set_box_cursor_events(layout, handle, {layout::BoxCursorEvents::Click});
      layout::set_box_offsets(layout, handle, xoff, 0);
      layout::end_group(layout);

      auto& pend = data.pending.emplace_back();
      pend.box_id = BOXIDI(handle);
      pend.quad_desc = ui::make_render_quad_desc(layout::read_box(layout, handle), Vec3f{0.5f});
      pend.left_down_callback = begin_drag_global_p_quantized;
      pend.color.x = px_span;
    }
    { //  reference semitone
      const auto* tuning = context.audio_component.get_ui_scale()->get_tuning();
      float ref_st = float(tuning->reference_semitone);
      if (data.dragging_reference_semitone) {
        ref_st = data.dragging_reference_semitone.value().candidate_value;
      }

      const float frac_val = clamp01((ref_st - Config::min_ref_st) / (Config::max_ref_st - Config::min_ref_st));
      const auto st_box = layout::read_box(layout, st_cont);

      const float handle_w = st_box.content_height();
      const float px_span = st_box.content_width() - handle_w;
      const float xoff = px_span * frac_val;

      layout::begin_group(layout, st_cont, layout::GroupOrientation::Manual, 0, 0, layout::JustifyContent::None);
      int handle = layout::box(layout, {1, handle_w, handle_w}, {1});
      layout::set_box_cursor_events(layout, handle, {layout::BoxCursorEvents::Click});
      layout::set_box_offsets(layout, handle, xoff, 0);
      layout::end_group(layout);

      auto& pend = data.pending.emplace_back();
      pend.box_id = BOXIDI(handle);
      pend.quad_desc = ui::make_render_quad_desc(layout::read_box(layout, handle), Vec3f{0.5f});
      pend.left_down_callback = begin_drag_reference_semitone;
      pend.color.x = px_span;
    }
    for (int nsi = 0; nsi < 2; nsi++) {
      //  note set
      const int min_nsi = pss::PitchSamplingParameters::min_note_set_index();
      const int max_nsi = pss::PitchSamplingParameters::max_note_set_index();
      const int si = nsi == 0 ?
        context.pitch_sampling_parameters.primary_note_set_index :
        context.pitch_sampling_parameters.secondary_note_set_index;
      int ref_nt = clamp(si, min_nsi, max_nsi);

      if (data.dragging_note_sets[nsi]) {
        ref_nt = int(data.dragging_note_sets[nsi].value().candidate_value);
      }

      float frac_val = float(ref_nt - min_nsi) / float(max_nsi - min_nsi);

      const auto ns_box = layout::read_box(layout, note_set_conts[nsi]);
      const float handle_w = ns_box.content_height();
      const float px_span = ns_box.content_width() - handle_w;
      const float xoff = px_span * frac_val;

      layout::begin_group(layout, note_set_conts[nsi], layout::GroupOrientation::Manual, 0, 0, layout::JustifyContent::None);
      int handle = layout::box(layout, {1, handle_w, handle_w}, {1});
      layout::set_box_cursor_events(layout, handle, {layout::BoxCursorEvents::Click});
      layout::set_box_offsets(layout, handle, xoff, 0);
      layout::end_group(layout);

      auto& pend = data.pending.emplace_back();
      pend.box_id = BOXIDI(handle);
      pend.quad_desc = ui::make_render_quad_desc(layout::read_box(layout, handle), Vec3f{0.5f});
      pend.left_down_callback = nsi == 0 ?
        begin_drag_primary_note_set_index : begin_drag_secondary_note_set_index;
      pend.color.x = px_span;
    }
#if 0
    { //  reference frequency
      const auto* tuning = context.audio_component.get_ui_scale()->get_tuning();
      float ref_freq = float(tuning->reference_frequency);
      if (data.dragging_reference_frequency) {
        ref_freq = data.dragging_reference_frequency.value().candidate_value;
      }

      const float frac_val = clamp01((ref_freq - Config::min_ref_freq) / (Config::max_ref_freq - Config::min_ref_freq));
      const auto freq_box = layout::read_box(layout, freq_cont);

      const float handle_w = freq_box.content_height();
      const float px_span = freq_box.content_width() - handle_w;
      const float xoff = px_span * frac_val;

      layout::begin_group(layout, freq_cont, layout::GroupOrientation::Manual, 0, 0, layout::JustifyContent::None);
      int handle = layout::box(layout, {1, handle_w, handle_w}, {1});
      layout::set_box_cursor_events(layout, handle, {layout::BoxCursorEvents::Click});
      layout::set_box_offsets(layout, handle, xoff, 0);
      layout::end_group(layout);

      auto& pend = data.pending.emplace_back();
      pend.box_id = BOXIDI(handle);
      pend.quad_desc = ui::make_render_quad_desc(layout::read_box(layout, handle), Vec3f{});
      pend.left_down_callback = begin_drag_reference_frequency;
      pend.color.x = px_span;
    }
#endif
  }

  const int num_tracks = int(note_clip_tracks.size());
  const int num_rows = num_tracks + 1;

  int sub_box{};
  {
    float scroll{};
    cursor::read_scroll_offsets(cursor_state, BOXIDI(top_panel), nullptr, &scroll);
    layout::begin_group(layout, top_panel, layout::GroupOrientation::Block, 0, scroll, layout::JustifyContent::Left);

    const int next_box = layout::next_box_index(layout);
    for (int i = 0; i < num_rows; i++) {
      layout::box(layout, {1.0f}, {1, 128, 128});
    }
    layout::end_group(layout);

    sub_box = layout::next_box_index(layout);
    for (int i = 0; i < num_rows; i++) {
      layout::begin_group(layout, next_box + i, {}, 0, 0, {}, {32, 32, 32, 32});
      layout::box(layout, {1}, {1});
      layout::end_group(layout);
    }

    for (int i = 0; i < num_tracks; i++) {
      const int ind = i + sub_box;
      auto box = layout::read_box(layout, ind);
      if (!box.is_clipped()) {
        auto& pend = data.pending.emplace_back();
        pend.box_id = BOXIDI(ind);
        pend.quad_desc = ui::make_render_quad_desc(box, Vec3f{1.0f});
        pend.quad_desc.value().translucency = 0.5f;
      }
    }

    { //  New track
      layout::begin_group(layout, sub_box + num_rows - 1, layout::GroupOrientation::Block, 0, 0, layout::JustifyContent::Left);
      int new_track = layout::box(layout, {1, 64, 64}, {1.0f});
      layout::end_group(layout);

      layout::begin_group(layout, new_track, layout::GroupOrientation::Col);
      int button = layout::box(layout, {0.5f}, {0.5f});
      layout::set_box_cursor_events(layout, button, {layout::BoxCursorEvents::Click});
      layout::end_group(layout);

      auto box = layout::read_box(layout, button);
      if (!box.is_clipped()) {
        auto& pend = data.pending.emplace_back();
        pend.box_id = BOXIDI(button);
        pend.quad_desc = ui::make_render_quad_desc(box, Vec3f{1.0f});
        pend.quad_desc.value().border_px = 4.0f;
        pend.quad_desc.value().linear_border_color = Vec3f{0.5f, 1.0f, 0.75f};
        pend.left_click_callback = create_note_clip_track;
      }
    }
  }

  {
    int box_off = layout::next_box_index(layout);
    for (int i = 0; i < num_tracks; i++) {
      layout::begin_group(layout, i + sub_box, layout::GroupOrientation::Block, 0, 0, layout::JustifyContent::Left);
      const int track_info = layout::box(layout, {0.125f}, {1.0f});  //  track info
      layout::set_box_cursor_events(layout, track_info, {layout::BoxCursorEvents::Click});
      int clip_cont = layout::box(layout, {1.0f - 0.125f}, {1.0f});  //  clips
      (void) clip_cont;
      layout::end_group(layout);

      {
        auto box = layout::read_box(layout, track_info);
        if (!box.is_clipped()) {
          auto& pend = data.pending.emplace_back();
          pend.is_track_selector = true;
          pend.box_id = BOXIDI(track_info);
          pend.quad_desc = ui::make_render_quad_desc(box, Vec3f{1.0f}, {}, {}, {}, 0.5f);
          pend.note_clip_track = note_clip_tracks[i].track_handle;
          pend.left_click_callback = select_track;
        }
      }
    }

    { //  ports / track info
      const int cont_off = layout::next_box_index(layout);
      for (int i = 0; i < num_tracks; i++) {
        const int track_info = box_off + i * 2;
        layout::begin_group(layout, track_info, layout::GroupOrientation::Row);
        int section0 = layout::box(layout, {1}, {0.5f});
        layout::set_box_cursor_events(layout, section0, {layout::BoxCursorEvents::Pass});
        int section1 = layout::box(layout, {1}, {0.5f});
        layout::set_box_cursor_events(layout, section1, {layout::BoxCursorEvents::Pass});
        layout::end_group(layout);
      }

      { //  ports
        const int pend_off = int(data.pending.size());
        for (int i = 0; i < num_tracks; i++) {
          const int outputs = cont_off + i * 2;
          layout::begin_group(layout, outputs, layout::GroupOrientation::Col);
          const auto& track = note_clip_tracks[i];
          auto process_it = ui_timeline_system.read_processor_nodes(track);
          for (; process_it != ui_timeline_system.end_processor_nodes(); ++process_it) {
            const AudioNodeStorage::NodeID node_id = *process_it;
            auto node_info = audio_component->audio_node_storage.get_port_info_for_node(node_id);
            assert(node_info);
            for (auto& port : node_info.value()) {
              const int bi = layout::box(layout, {1, 16, 16}, {1, 16, 16});
              layout::set_box_cursor_events(layout, bi, {layout::BoxCursorEvents::Click});
              auto& pend = data.pending.emplace_back();
              pend.box_id = BOXIDI(bi);
              pend.port_id = port.id;
              pend.connected = port.connected();
              pend.left_click_callback = select_port;
            }
          }
          layout::end_group(layout);
        }

        const int num_evaluate = int(data.pending.size());
        for (int i = pend_off; i < num_evaluate; i++) {
          auto& pend = data.pending[i];
          pend.quad_desc = ui::make_render_quad_desc(
            layout::read_box(layout, pend.box_id.index()),
            color_for_data_type(AudioNodeStorage::DataType::MIDIMessage));
          if (pend.connected) {
            auto eval_box = layout::evaluate_clipped_box_centered(
              layout, pend.box_id.index(), {0.5f}, {0.5f});
            if (!eval_box.is_clipped()) {
              auto& next = data.pending.emplace_back();
              next.quad_desc = ui::make_render_quad_desc(eval_box, Vec3f{1.0f});
            }
          }
        }
      }

      { //  track info
        const int pend_off = int(data.pending.size());
        for (int i = 0; i < num_tracks; i++) {
          const int info = cont_off + i * 2 + 1;
          layout::begin_group(layout, info, layout::GroupOrientation::Col);
          const auto& track = note_clip_tracks[i];

          {
            const int bi = layout::box(layout, {1, 16, 16}, {1, 16, 16});
            layout::set_box_cursor_events(layout, bi, {layout::BoxCursorEvents::Click});

            auto& pend = data.pending.emplace_back();
            auto color = color_for_data_type(AudioNodeStorage::DataType::MIDIMessage);
            pend.box_id = BOXIDI(bi);
            pend.color = track.midi_output_enabled ? color * 0.25f : color;
            pend.left_click_callback = toggle_midi_output_enabled;
            pend.note_clip_track = track.track_handle;
          }
          {
            const int bi = layout::box(layout, {1, 16, 16}, {1, 16, 16});
            layout::set_box_cursor_events(layout, bi, {layout::BoxCursorEvents::Click});

            auto& pend = data.pending.emplace_back();
            auto color = Vec3f{1.0f, 0.0f, 0.0f};
            pend.box_id = BOXIDI(bi);
            pend.color = track.is_recording ? color * 0.25f : color;
            pend.left_click_callback = toggle_midi_recording;
            pend.note_clip_track = track.track_handle;
          }
          {
            const int bi = layout::box(layout, {1, 16, 16}, {1, 16, 16});
            layout::set_box_cursor_events(layout, bi, {layout::BoxCursorEvents::Click});

            auto& pend = data.pending.emplace_back();
            pend.box_id = BOXIDI(bi);
            pend.color = Vec3f{0.0f, 0.0f, 1.0f};
            pend.note_clip_track = track.track_handle;
            pend.left_click_callback = create_note_clip;
          }
          {
            const int bi = layout::box(layout, {1, 16, 16}, {1, 16, 16});
            layout::set_box_cursor_events(layout, bi, {layout::BoxCursorEvents::Click});

            auto color = Vec3f{0.0f, 1.0f, 0.0f};
            auto& pend = data.pending.emplace_back();
            pend.box_id = BOXIDI(bi);
            pend.color = track.arp_output_enabled ? color * 0.25f : color;
            pend.note_clip_track = track.track_handle;
            pend.left_click_callback = toggle_arp_enabled;
          }

          layout::end_group(layout);
        }

        for (int i = pend_off; i < int(data.pending.size()); i++) {
          auto& pend = data.pending[i];
          pend.quad_desc = ui::make_render_quad_desc(
            layout::read_box(layout, pend.box_id.index()), pend.color);
          pend.quad_desc.value().radius_fraction = 1.0f;
        }
      }
    }

    int clip_cont_off{};
    {
      clip_cont_off = layout::next_box_index(layout);
      for (int i = 0; i < num_tracks; i++) {
        const int clip_container = box_off + i * 2 + 1;
        const float cont_height = layout::read_box(layout, clip_container).content_height();
        layout::begin_group(layout, clip_container, layout::GroupOrientation::Row);
        layout::box(layout, {1}, {1, 16, 16});                                  //  track region header
        layout::box(layout, {1}, {1, cont_height - 16, cont_height - 16});      //  clip container
        layout::end_group(layout);
      }
    }

    { //  loop region
      for (int i = 0; i < num_tracks; i++) {
        const int track_header = clip_cont_off + i * 2;

        const auto& track = note_clip_tracks[i];
        auto& view = data.track_views[track_view_indices[i]];

        const float track_w = gui::layout::read_box(layout, track_header).content_width();
        const double view_beg = as_beats(view.region.begin);
        const double view_size = as_beats(view.region.size);
        view.last_box_width = track_w;

        const auto* timeline_track = ui_read_note_clip_track(
          &audio_component->timeline_system, track.track_handle);

        if (timeline_track->loop_region) {
          auto loop = timeline_track->loop_region.value();

          if (data.dragging_loop_size &&
              data.dragging_loop_size.value().track == timeline_track->handle) {
            loop = data.dragging_loop_size.value().candidate_region;
          } else if (data.dragging_loop_offset &&
                     data.dragging_loop_offset.value().track == timeline_track->handle) {
            loop = data.dragging_loop_offset.value().candidate_region;
          }

          auto loop_beg = as_beats(loop.begin);
          auto loop_size = as_beats(loop.size);
          double loop_beg_px = ((loop_beg - view_beg) / view_size) * track_w;
          auto loop_size_px = float((loop_size / view_size) * track_w);

          layout::begin_group(layout, track_header, layout::GroupOrientation::Manual, 0, 0, layout::JustifyContent::None);
          int bi = layout::box(layout, {1, loop_size_px, loop_size_px}, {1});
          layout::set_box_cursor_events(layout, bi, {layout::BoxCursorEvents::Click});
          layout::set_box_offsets(layout, bi, float(loop_beg_px), 0);
          layout::end_group(layout);

          {
            auto box = layout::read_box(layout, bi);
            if (!box.is_clipped()) {
              auto& pend = data.pending.emplace_back();
              pend.box_id = BOXIDI(bi);
              pend.quad_desc = ui::make_render_quad_desc(box, Vec3f{1.0f, 1.0f, 0.0f});
              pend.note_clip_track = timeline_track->handle;
              pend.left_down_callback = begin_drag_loop_offset;
            }
          }

          layout::begin_group(layout, bi, layout::GroupOrientation::Col, 0, 0, layout::JustifyContent::Right);
          const int handle = layout::box(layout, {0.25f, 16, 16}, {1});
          layout::set_box_cursor_events(layout, handle, {layout::BoxCursorEvents::Click});
          layout::end_group(layout);
          {
            auto box = layout::read_box(layout, handle);
            if (!box.is_clipped()) {
              auto& pend = data.pending.emplace_back();
              pend.box_id = BOXIDI(handle);
              pend.quad_desc = ui::make_render_quad_desc(box, Vec3f{1.0f, 0.0f, 1.0f});
              pend.note_clip_track = timeline_track->handle;
              pend.left_down_callback = begin_drag_loop_size;
            }
          }
        }
      }
    }

    { //  clips
      const int pend_off = int(data.pending.size());
      const int clip_box_off = layout::next_box_index(layout);
      for (int i = 0; i < num_tracks; i++) {
        const int clip_container = clip_cont_off + i * 2 + 1;

        const auto& track = note_clip_tracks[i];
        auto& view = data.track_views[track_view_indices[i]];

        const float track_w = gui::layout::read_box(layout, clip_container).content_width();
        const double view_beg = as_beats(view.region.begin);
        const double view_size = as_beats(view.region.size);

        float scroll{};
        cursor::read_scroll_offsets(cursor_state, BOXIDI(clip_container), nullptr, &scroll);
        const float px_per_beat = track_w / float(view_size);
        view.region.begin = ScoreCursor::from_beats(
          std::floor(scroll / px_per_beat), reference_time_signature().numerator);

        layout::begin_group(layout, clip_container, layout::GroupOrientation::Manual, 0, 0, layout::JustifyContent::None);
        const auto* timeline_track = ui_read_note_clip_track(
          &audio_component->timeline_system, track.track_handle);

        for (auto& clip_handle : timeline_track->clips) {
          auto* clip = ui_read_clip(audio_component->timeline_system.clip_system, clip_handle);
          assert(clip);
          auto clip_span = clip->span;
          if (data.dragging_clip_offset && data.dragging_clip_offset.value().clip == clip_handle) {
            clip_span = data.dragging_clip_offset.value().candidate_region;
          } else if (data.dragging_clip_size && data.dragging_clip_size.value().clip == clip_handle) {
            clip_span = data.dragging_clip_size.value().candidate_region;
          }

          double clip_beg = as_beats(clip_span.begin);
          auto clip_size = as_beats(clip_span.size);
          double clip_x0 = ((clip_beg - view_beg) / view_size) * track_w;
          auto clip_w = float((clip_size / view_size) * track_w);

          const int bi = layout::box(layout, {1, clip_w, clip_w}, {1.0f});
          layout::set_box_offsets(layout, bi, float(clip_x0), 0);
          layout::set_box_cursor_events(layout, bi, {layout::BoxCursorEvents::Click});
          auto& pend = data.pending.emplace_back();
          pend.box_id = BOXIDI(bi);
          pend.note_clip = clip_handle;
          pend.note_clip_track = timeline_track->handle;
//          pend.left_click_callback = activate_clip;
          pend.left_click_callback = select_and_activate_clip;
        }
        layout::end_group(layout);
      }

      for (int i = pend_off; i < int(data.pending.size()); i++) {
        auto& pend = data.pending[i];
        auto box = layout::read_box(layout, pend.box_id.index());
        if (!box.is_clipped()) {
          pend.quad_desc = ui::make_render_quad_desc(box, Vec3f{0.0f, 0.0f, 1.0f});
          if (data.active_clip_view && data.active_clip_view.value().clip == pend.note_clip) {
            pend.quad_desc.value().border_px = 2.0f;
          }
        }
      }

      const int num_clips = int(data.pending.size()) - pend_off;
      for (int i = 0; i < num_clips; i++) {
        const auto track_handle = data.pending[i + pend_off].note_clip_track;
        const auto clip_handle = data.pending[i + pend_off].note_clip;

        const int clip_box_ind = clip_box_off + i;
        const auto clip_box = layout::read_box(layout, clip_box_ind);
        layout::begin_group(layout, clip_box_ind, layout::GroupOrientation::Manual, 0, 0, layout::JustifyContent::None);

        const float handle_h = 16.0f;
        const float handle_w = 16.0f;
        int handle = layout::box(layout, {1, handle_w, handle_w}, {1, handle_h, handle_h});
        layout::set_box_cursor_events(layout, handle, {layout::BoxCursorEvents::Click});
        layout::set_box_offsets(layout, handle, clip_box.content_width() - handle_w, clip_box.content_height() - handle_h);

        int rest = layout::box(layout, {1}, {1, handle_h, handle_h});
        layout::set_box_cursor_events(layout, rest, {layout::BoxCursorEvents::Click});
        layout::set_box_offsets(layout, rest, 0, clip_box.content_height() - handle_h);

#if 0
        const float sd = 8.0f;
        int sel = layout::box(layout, {1, sd, sd}, {1, sd, sd});
        layout::set_box_cursor_events(layout, sel, {layout::BoxCursorEvents::Click});
        const float sel_x0 = clip_box.content_width() * 0.5f - sd * 0.5f;
        const float sel_y0 = (clip_box.content_height() - handle_h) * 0.5f - sd * 0.5f;
        layout::set_box_offsets(layout, sel, sel_x0, sel_y0);
#endif
        layout::end_group(layout);
#if 0
        {
          auto box = layout::read_box(layout, sel);
          if (!box.is_clipped()) {
            auto& pend = data.pending.emplace_back();
            pend.box_id = BOXIDI(sel);
            pend.quad_desc = ui::make_render_quad_desc(box, Vec3f{1.0f});
            pend.note_clip = clip_handle;
            pend.note_clip_track = track_handle;
            pend.left_click_callback = select_clip;
          }
        }
#endif

        {
          auto box = layout::read_box(layout, rest);
          if (!box.is_clipped()) {
            auto& pend = data.pending.emplace_back();
            pend.box_id = BOXIDI(rest);
            pend.quad_desc = ui::make_render_quad_desc(box, Vec3f{1.0f, 0.0f, 0.0f});
            pend.left_down_callback = begin_drag_clip_offset;
            pend.note_clip_track = track_handle;
            pend.note_clip = clip_handle;
          }
        }
        {
          auto box = layout::read_box(layout, handle);
          if (!box.is_clipped()) {
            auto& pend = data.pending.emplace_back();
            pend.box_id = BOXIDI(handle);
            pend.quad_desc = ui::make_render_quad_desc(box, Vec3f{0.0f, 1.0f, 0.0f});
            pend.left_down_callback = begin_drag_clip_size;
            pend.note_clip_track = track_handle;
            pend.note_clip = clip_handle;
          }
        }
      }
    }
    { //  cursor
      for (int i = 0; i < num_tracks; i++) {
        auto track_cont = layout::read_box(layout, box_off + i * 2 + 1);
        const auto th = note_clip_tracks[i].track_handle;
        auto* track = ui_read_note_clip_track(&context.audio_component.timeline_system, th);
        assert(track);
        auto cursor_pos = track->ui_approximate_cursor_position;
        auto& view = data.track_views[track_view_indices[i]];
        auto off = (as_beats(cursor_pos) - as_beats(view.region.begin)) / as_beats(view.region.size);
        auto px_off = track_cont.content_width() * off;
        layout::ReadBox box{};
        box.x0 = track_cont.x0 + float(px_off);
        box.x1 = box.x0 + 2.0f;
        box.y0 = track_cont.y0;
        box.y1 = track_cont.y1;
        box.set_clipping_rect_from_full_rect();
        track_cont.as_clipping_rect(&box.clip_x0, &box.clip_y0, &box.clip_x1, &box.clip_y1);
        if (!box.is_clipped()) {
          auto& pend = data.pending.emplace_back();
          pend.quad_desc = ui::make_render_quad_desc(box, Vec3f{});
        }
      }
    }
  }

  int note_section;
  int top_section;
  {
    layout::begin_group(layout, bot_panel, layout::GroupOrientation::Row);
    top_section = layout::box(layout, {1}, {0.125f * 0.25f});
    note_section = layout::box(layout, {1}, {1.0f - 0.125f * 0.25f});
    layout::set_box_cursor_events(layout, note_section, {layout::BoxCursorEvents::Scroll});
    layout::end_group(layout);
  }

  if (data.active_clip_view) {  //  loop overlay
    const auto& clip_view = data.active_clip_view.value();

    const auto top_box = layout::read_box(layout, top_section);
    const float group_w = top_box.content_width();

    auto view_beg = as_beats(active_note_clip->span.begin);
    auto view_size = as_beats(clip_view.span_size);
    auto* track = ui_read_note_clip_track(&audio_component->timeline_system, clip_view.track);
    assert(track);

    if (track->loop_region) {
      auto loop_reg = track->loop_region.value();
      if (data.dragging_loop_offset && data.dragging_loop_offset.value().track == clip_view.track) {
        loop_reg = data.dragging_loop_offset.value().candidate_region;
      } else if (data.dragging_loop_size && data.dragging_loop_size.value().track == clip_view.track) {
        loop_reg = data.dragging_loop_size.value().candidate_region;
      }

      auto x0 = (as_beats(loop_reg.begin) - view_beg) / view_size * group_w;
      auto s = as_beats(loop_reg.size) / view_size * group_w;

      layout::ReadBox loop_box{};
      loop_box.x0 = float(top_box.x0 + x0);
      loop_box.x1 = float(top_box.x0 + x0 + s);
      loop_box.y0 = top_box.y0;
      loop_box.y1 = top_box.y1;
      loop_box.set_clipping_rect_from_full_rect();
      top_box.as_clipping_rect(&loop_box.clip_x0, &loop_box.clip_y0, &loop_box.clip_x1, &loop_box.clip_y1);
      if (!loop_box.is_clipped()) {
        auto& pend = data.pending.emplace_back();
        pend.quad_desc = ui::make_render_quad_desc(loop_box, Vec3f{1.0f, 1.0f, 0.0f});
      }
    }
  }

  if (data.active_clip_view) { //  notes
    const auto bot_box = layout::read_box(layout, note_section);
    const float group_w = bot_box.content_width();
    const float group_h = bot_box.content_height();

    auto& clip_view = data.active_clip_view.value();

    float view_st_size = clip_view.semitone_span.y - clip_view.semitone_span.x;
    float scroll{};
    cursor::read_scroll_offsets(cursor_state, BOXIDI(note_section), nullptr, &scroll);
    const float px_per_st = group_h / view_st_size;
    clip_view.semitone_span.x = std::floor(scroll / px_per_st) - 12.0f;
    clip_view.semitone_span.y = clip_view.semitone_span.x + view_st_size;

    auto view_beg = as_beats(active_note_clip->span.begin);
    auto view_size = as_beats(clip_view.span_size);
    double view_st_beg = clip_view.semitone_span.x;

    auto* track = ui_read_note_clip_track(&audio_component->timeline_system, clip_view.track);
    assert(track);
    const auto cursor_off = float(
      (as_beats(track->ui_approximate_cursor_position) - view_beg) / view_size * group_w);
    if (cursor_off >= 0.0f) {
      const float cursor_w = 4.0f;
      layout::ReadBox cursor_box{};
      cursor_box.x0 = bot_box.x0 + cursor_off;
      cursor_box.x1 = bot_box.x0 + cursor_off + cursor_w;
      cursor_box.y0 = bot_box.y0;
      cursor_box.y1 = bot_box.y1;
      cursor_box.set_clipping_rect_from_full_rect();
      bot_box.as_clipping_rect(&cursor_box.clip_x0, &cursor_box.clip_y0, &cursor_box.clip_x1, &cursor_box.clip_y1);
      if (!cursor_box.is_clipped()) {
        auto& pend = data.pending.emplace_back();
        pend.quad_desc = ui::make_render_quad_desc(cursor_box, Vec3f{1.0f, 0.0f, 0.0f});
      }
    }

    constexpr int num_stack_notes = 1024;
    Temporary<ClipNote, num_stack_notes> store_clip_notes;
    Temporary<uint32_t, num_stack_notes> store_clip_note_inds;
    auto* clip_notes = store_clip_notes.require(num_stack_notes);
    auto* clip_note_inds = store_clip_note_inds.require(num_stack_notes);

    ScoreRegion sel_region{{}, clip_view.span_size};
    int num_notes = ui_collect_notes_intersecting_region(
      audio_component->timeline_system.clip_system, active_note_clip, sel_region,
      clip_note_inds, clip_notes, num_stack_notes);

    if (num_notes > num_stack_notes) {
      clip_notes = store_clip_notes.require(num_notes);
      clip_note_inds = store_clip_note_inds.require(num_notes);
      ui_collect_notes_intersecting_region(
        audio_component->timeline_system.clip_system, active_note_clip, sel_region,
        clip_note_inds, clip_notes, num_stack_notes);
    }

    layout::begin_group(layout, note_section, gui::layout::GroupOrientation::Manual, 0, 0, gui::layout::JustifyContent::None);
    const int next_box = layout::next_box_index(layout);
    for (int i = 0; i < num_notes; i++) {
      auto note_span = clip_notes[i].span;
      auto note_beg = as_beats(note_span.begin);
      auto note_size = as_beats(note_span.size);
      double note_st = clip_notes[i].note.semitone();

      double x0 = ((note_beg) / view_size) * group_w;
      auto note_w = float((note_size / view_size) * group_w);
      double y0 = (1.0f - ((note_st - view_st_beg) / view_st_size)) * group_h;
      auto note_h = float((1.0 / view_st_size) * group_h);
      int bi = layout::box(layout, {1, note_w, note_w}, {1, note_h, note_h});
      layout::set_box_offsets(layout, bi, float(x0), float(y0));
      layout::set_box_cursor_events(layout, bi, {layout::BoxCursorEvents::Click});
    }
    layout::end_group(layout);

    for (int i = 0; i < num_notes; i++) {
      const int ind = next_box + i;
      auto box = layout::read_box(layout, ind);
      if (!box.is_clipped()) {
        auto& pend = data.pending.emplace_back();
        pend.box_id = BOXIDI(ind);
        pend.quad_desc = ui::make_render_quad_desc(box, Vec3f{1.0f});
        pend.clip_note = clip_notes[i];
        pend.left_click_callback = select_note;
      }
    }
  }

  //  end
  const int num_boxes = layout::total_num_boxes(layout);
  cursor::evaluate_boxes(cursor_state, 2, layout::read_box_slot_begin(layout), num_boxes);
}

void evaluate(AudioTimelineEditorData& data, const CommonContext& context) {
  auto* cursor_state = &context.cursor_state;
  for (auto& pend : data.pending) {
    if (pend.left_click_callback && cursor::left_clicked_on(cursor_state, pend.box_id)) {
      pend.left_click_callback(pend, data, context);
    }
    if (pend.left_down_callback && cursor::newly_left_down_on(cursor_state, pend.box_id)) {
      pend.left_down_callback(pend, data, context);
    }
  }

  const bool left_released = context.mouse_button_trigger.newly_released(Mouse::Button::Left);
  if (data.dragging_loop_size && left_released) {
    auto& drag = data.dragging_loop_size.value();
    auto& tsys = context.audio_component.timeline_system;
    ui_set_track_loop_region(&tsys, drag.track, drag.candidate_region);
    data.dragging_loop_size = NullOpt{};
  }

  if (data.dragging_loop_offset && left_released) {
    auto& drag = data.dragging_loop_offset.value();
    auto& tsys = context.audio_component.timeline_system;
    ui_set_track_loop_region(&tsys, drag.track, drag.candidate_region);
    data.dragging_loop_offset = NullOpt{};
  }

  if (data.dragging_clip_offset && left_released) {
    auto& drag = data.dragging_clip_offset.value();
    auto& tsys = context.audio_component.timeline_system;
    ui_set_timeline_note_clip_span(&tsys, drag.track, drag.clip, drag.candidate_region);
    data.dragging_clip_offset = NullOpt{};
  }

  if (data.dragging_clip_size && left_released) {
    auto& drag = data.dragging_clip_size.value();
    auto& tsys = context.audio_component.timeline_system;
    ui_set_timeline_note_clip_span(&tsys, drag.track, drag.clip, drag.candidate_region);
    data.dragging_clip_size = NullOpt{};
  }

  if (data.dragging_bpm) {
    context.audio_component.audio_transport.set_bpm(data.dragging_bpm.value().candidate_value);
    if (left_released) {
      data.dragging_bpm = NullOpt{};
    }
  }

  if (data.dragging_global_p_quantized) {
    context.rhythm_parameters.set_global_p_quantized(
      data.dragging_global_p_quantized.value().candidate_value);
    if (left_released) {
      data.dragging_global_p_quantized = NullOpt{};
    }
  }

  if (data.dragging_reference_semitone) {
    auto* ui_scale = context.audio_component.get_ui_scale();
    auto tuning = *ui_scale->get_tuning();
    tuning.reference_semitone = data.dragging_reference_semitone.value().candidate_value;
    ui_scale->set_tuning(tuning);
    if (left_released) {
      data.dragging_reference_semitone = NullOpt{};
    }
  }

  for (int i = 0; i < 2; i++) {
    if (data.dragging_note_sets[i]) {
      int val = int(data.dragging_note_sets[i].value().candidate_value);
      context.pitch_sampling_parameters.set_ith_note_set_index(
        context.audio_component.get_pitch_sampling_system(),
        context.audio_component.get_audio_scale_system(), i, val);
      if (left_released) {
        data.dragging_note_sets[i] = NullOpt{};
      }
    }
  }

  const bool alt_pressed = context.key_trigger.is_pressed(Key::LeftAlt);

  if (data.copied_clip && alt_pressed && context.key_trigger.newly_pressed(Key::V) &&
      !data.selected_note_clip_tracks.empty()) {
    auto dst = data.selected_note_clip_tracks.back();
    ui_paste_timeline_note_clip_at_end(
      &context.audio_component.timeline_system, dst, data.copied_clip.value().clip);
  }

  if (!data.selected_note_clips.empty() && alt_pressed && context.key_trigger.newly_pressed(Key::C)) {
    data.copied_clip = data.selected_note_clips[0];
  }

  if (!data.selected_note_clips.empty() && alt_pressed && context.key_trigger.newly_pressed(Key::D)) {
    auto& sel = data.selected_note_clips[0];
    ui_duplicate_timeline_note_clip(
      &context.audio_component.timeline_system, sel.track, sel.clip);
  }

  const bool bs_pressed = context.key_trigger.newly_pressed(Key::Backspace);
  if (bs_pressed && data.active_clip_view) {
    auto* clip_sys = context.audio_component.timeline_system.clip_system;
    auto& sel = data.selected_clip_notes;
    auto& view = data.active_clip_view.value();
    ui_remove_existing_notes(clip_sys, view.clip, sel.data(), int(sel.size()));
    sel.clear();
  }
  if (bs_pressed) {
    for (auto& sel : data.selected_note_clips) {
      ui_destroy_timeline_note_clip(&context.audio_component.timeline_system, sel.track, sel.clip);
    }
    data.selected_note_clips.clear();
  }
  if (bs_pressed) {
    auto& audio_component = context.audio_component;
    for (auto& sel : data.selected_note_clip_tracks) {
      audio_component.ui_timeline_system.destroy_note_clip_track(
        sel,
        *audio_component.get_timeline_system(),
        *audio_component.get_triggered_notes(),
        *audio_component.get_arpeggiator_system(),
        audio_component.audio_connection_manager);
    }
    data.selected_note_clip_tracks.clear();
  }
}

void render(AudioTimelineEditorData& data, const CommonContext& context) {
  const auto* cursor_state = &context.cursor_state;
  for (auto& pend : data.pending) {
    if (pend.quad_desc) {
      if (context.selected.contains(pend.port_id) ||
          has_selected_note(data, pend) ||
          has_selected_clip(data, pend) ||
          has_selected_track(data, pend) ||
          cursor::left_down_on(cursor_state, pend.box_id)) {
        pend.quad_desc.value().linear_color *= 0.75f;

      } else if (cursor::hovered_over(cursor_state, pend.box_id)) {
        pend.quad_desc.value().linear_color *= 0.5f;
      }
      gui::draw_quads(&context.render_data, &pend.quad_desc.value(), 1);
    }
  }
}

struct {
  AudioTimelineEditorData data;
} globals;

} //  anon

void ui::prepare_audio_timeline_editor(const CommonContext& context) {
  prepare(globals.data, context);
}

void ui::evaluate_audio_timeline_editor(const CommonContext& context) {
  evaluate(globals.data, context);
}

void ui::render_audio_timeline_editor(const CommonContext& context) {
  render(globals.data, context);
}

void ui::destroy_audio_timeline_editor() {
  layout::destroy_layout(&globals.data.layout);
}

#undef BOXIDI

GROVE_NAMESPACE_END
