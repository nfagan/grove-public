#include "AudioParameterSystem.hpp"
#include "AudioEventSystem.hpp"
#include "audio_events.hpp"
#include "Transport.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include "grove/common/Handshake.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/common/RingBuffer.hpp"
#include <unordered_map>
#include <unordered_set>

GROVE_NAMESPACE_BEGIN

namespace {

template <typename T>
using ParamIDMap = std::unordered_map<AudioParameterIDs, T, AudioParameterIDs::Hash>;
using ParamIDSet = std::unordered_set<AudioParameterIDs, AudioParameterIDs::Hash>;

struct RenderFeedbackItem {
  enum class Type {
    CursorLocation = 0,
  };

  struct CursorLocation {
    ScoreCursor position;
  };

  Type type;
  union {
    CursorLocation cursor_location;
  };
};

struct RenderData {
  AudioParameterChanges changes;
  double last_bpm{};
  RingBuffer<RenderFeedbackItem, 16> feedback_items;
  std::atomic<bool> emit_events{};
};

struct AudioParameterModification {
  struct SetValue {
    AudioParameterIDs ids;
    AudioParameterValue value;
  };
  struct RevertToBreakPoints {
    AudioParameterIDs ids;
  };
  struct RemoveParent {
    AudioParameterID parent;
  };

  enum class Type {
    None = 0,
    SetValue,
    RevertToBreakPoints,
    RemoveParent,
  };

  Type type;
  union {
    SetValue set_value;
    RevertToBreakPoints revert_to_break_points;
    RemoveParent remove_parent;
  };
};

struct InstanceBreakPointSet {
  BreakPointSetHandle handle{};
  BreakPointSet set;
};

struct BreakPointSetModification {
  struct CreateOrDestroySet {
    BreakPointSetHandle handle;
    ScoreRegion span;
  };

  struct AddOrRemovePoint {
    BreakPointSetHandle set;
    AudioParameterDescriptor param_desc;
    BreakPoint point;
  };

  struct ModifyPoint {
    BreakPointSetHandle set;
    AudioParameterDescriptor param_desc;
    BreakPoint point;
  };

  struct RemoveParent {
    BreakPointSetHandle set;
    AudioParameterID parent;
  };

  enum class Type {
    None = 0,
    AddPoint,
    RemovePoint,
    ModifyPoint,
    CreateSet,
    DestroySet,
    RemoveParent
  };

  Type type;
  union {
    AddOrRemovePoint add_or_remove_point;
    ModifyPoint modify_point;
    CreateOrDestroySet create_or_destroy_set;
    RemoveParent remove_parent;
  };
};

struct ParameterInstanceData {
  ParamIDSet controlled_by_ui;
  ParamIDMap<AudioParameterValue> ui_values;
};

struct ParameterStateChanges {
  bool empty() const {
    return newly_set_values.empty() &&
           newly_reverted_to_break_points.empty() &&
           need_resynchronize.empty();
  }

  void clear() {
    newly_set_values.clear();
    newly_reverted_to_break_points.clear();
    need_resynchronize.clear();
  }

  void remove_parent(AudioParameterID id);

  ParamIDSet newly_set_values;
  ParamIDSet newly_reverted_to_break_points;
  ParamIDSet need_resynchronize;
};

struct BreakPointInstanceData {
  std::vector<std::unique_ptr<InstanceBreakPointSet>> break_point_sets;
  InstanceBreakPointSet* active_set{};
};

struct InstanceData {
  bool is_ui_controlled(AudioParameterIDs ids) const {
    return parameter_instance.controlled_by_ui.count(ids) > 0;
  }

  ParameterStateChanges parameter_state_changes;
  ParameterInstanceData parameter_instance;

  BreakPointInstanceData break_point_instance;
  bool break_points_modified{};

  std::shared_ptr<RenderData> render_data;
};

struct BlockInfo {
  TimeSignature tsig;
  double bpm;
  double beats_per_sample;
  double samples_per_beat;
  ScoreCursor size;
  ScoreCursor cursor;
};

struct PartitionedBlock {
  static constexpr int interval_stack_size = 32;

  ScoreCursor begin;
  Temporary<ScoreRegionSegment, interval_stack_size> interval_store;
  ScoreRegionSegment* intervals;
  int num_intervals;
};

struct GridPoints {
  const BreakPoint* prev;
  const BreakPoint* next;
  ScoreCursor to_prev;
  ScoreCursor to_next;
  ScoreCursor tot_distance;
};

struct ResynchronizingParameterChanges {
  AudioParameterChange changes[2];
  int num_changes;
};

[[maybe_unused]] constexpr const char* logging_id() {
  return "AudioParameterSystem";
}

RenderFeedbackItem make_cursor_location_render_feedback_item(ScoreCursor position) {
  RenderFeedbackItem item{};
  item.type = RenderFeedbackItem::Type::CursorLocation;
  item.cursor_location = {};
  item.cursor_location.position = position;
  return item;
}

void render_maybe_push_feedback_item(RenderData& data, const RenderFeedbackItem& item) {
  data.feedback_items.maybe_write(item);
}

void remove_matching_parent(ParamIDSet& set, AudioParameterID parent) {
  auto it = set.begin();
  while (it != set.end()) {
    if (it->parent == parent) {
      it = set.erase(it);
    } else {
      ++it;
    }
  }
}

template <typename T>
void remove_matching_parent(ParamIDMap<T>& map, AudioParameterID parent) {
  auto it = map.begin();
  while (it != map.end()) {
    if (it->first.parent == parent) {
      it = map.erase(it);
    } else {
      ++it;
    }
  }
}

void ParameterStateChanges::remove_parent(AudioParameterID id) {
  remove_matching_parent(newly_set_values, id);
  remove_matching_parent(newly_reverted_to_break_points, id);
  remove_matching_parent(need_resynchronize, id);
}

const BreakPoint* next_it(const BreakPoint* beg, const BreakPoint* end, const BreakPoint* it) {
  assert(beg != end);
  return it == (end - 1) ? beg : it + 1;
}

const BreakPoint* prev_it(const BreakPoint* beg, const BreakPoint* end, const BreakPoint* it) {
  assert(beg != end);
  return it == beg ? end - 1 : it - 1;
}

GridPoints surrounding_points(const BreakPoint* beg, const BreakPoint* end,
                              ScoreCursor cursor, const ScoreRegion& set_span, double num) {
  assert(beg != end);

  auto* next = first_gt(beg, end, cursor);
  next = next == end ? beg : next;
  auto* prev = prev_it(beg, end, next);

  auto to_next = lt_order_dependent_cursor_distance(
    cursor, next->position, set_span, num);

  auto to_prev = le_order_dependent_cursor_distance(
    prev->position, cursor, set_span, num);

  auto tot_dist = to_prev;
  tot_dist.wrapped_add_cursor(to_next, num);

  GridPoints result{};
  result.prev = prev;
  result.next = next;
  result.to_prev = to_prev;
  result.to_next = to_next;
  result.tot_distance = tot_dist;
  return result;
}

GridPoints surrounding_points(const BreakPointSet::BreakPointsByParameter& param, ScoreCursor cursor,
                              const ScoreRegion& set_span, double num) {
  return surrounding_points(
    param.points.data(),
    param.points.data() + param.points.size(),
    cursor, set_span, num);
}

AudioParameterValue lerp(const GridPoints& points, double num) {
  if (points.prev->value.is_float()) {
    auto tot_dist = points.tot_distance.to_beats(num);
    auto to_prev = points.to_prev.to_beats(num);
    const float t = clamp01(float(to_prev / tot_dist));
    return parameter_lerp(t, points.prev->value, points.next->value);
  } else {
    return points.prev->value;
  }
}

void partition_block(PartitionedBlock* dst, const ScoreRegion& set_span, const BlockInfo& info) {
  const int interval_stack_size = PartitionedBlock::interval_stack_size;
  ScoreRegionSegment* intervals = dst->interval_store.require(interval_stack_size);

  const auto set_beg = set_span.loop(info.cursor, info.tsig.numerator);
  const int num_intervals = partition_loop(
    ScoreRegion{set_beg, info.size},
    set_span, info.tsig.numerator, intervals, interval_stack_size);
  assert(num_intervals <= interval_stack_size);

  dst->begin = set_beg;
  dst->num_intervals = std::min(num_intervals, interval_stack_size);
  dst->intervals = intervals;
}

BlockInfo get_block_info(const Transport& transport, double sample_rate, int num_frames) {
  const auto cursor_begin = transport.render_get_cursor_location();
  const auto tsig = reference_time_signature();
  const double bpm = transport.get_bpm();
  const double bps = beats_per_sample_at_bpm(bpm, sample_rate, tsig);
  const double spb = 1.0 / bps;
  BlockInfo result{};
  result.tsig = tsig;
  result.bpm = bpm;
  result.beats_per_sample = bps;
  result.samples_per_beat = spb;
  result.size = ScoreCursor::from_beats(bps * double(num_frames), tsig.numerator);
  result.cursor = cursor_begin;
  return result;
}

AudioParameterChange make_immediate_change(const AudioParameterIDs& ids,
                                           const AudioParameterValue& value) {
  return AudioParameterChange{ids, value, 0, 0};
}

int interpolating_frame_distance(const BreakPoint& p0, const BreakPoint& p1,
                                 const ScoreRegion& set_span, double num, double samples_per_beat) {
  auto dist = lt_order_dependent_cursor_distance(p0.position, p1.position, set_span, num);
  double frame_dist = dist.to_sample_offset(samples_per_beat, num);
  return std::max(0, int(std::floor(frame_dist)));
}

auto resynchronizing_changes_stopped(const GridPoints& pts, const AudioParameterDescriptor& desc,
                                     double samples_per_beat, double num) {
  ResynchronizingParameterChanges result{};

  if (desc.is_float()) {
    const double tot_dist = std::max(
      1.0, pts.tot_distance.to_sample_offset(samples_per_beat, num));
    const int to_prev = int(
      std::max(0.0, pts.to_prev.to_sample_offset(samples_per_beat, num)));
    const double target_t = clamp01(double(to_prev) / tot_dist);
    auto target_val = parameter_lerp(float(target_t), pts.prev->value, pts.next->value);
    result.changes[result.num_changes++] = make_immediate_change(desc.ids, target_val);
  } else {
    result.changes[result.num_changes++] = make_immediate_change(desc.ids, pts.prev->value);
  }

  return result;
}

auto resynchronizing_changes_playing(const GridPoints& pts, const AudioParameterDescriptor& desc,
                                     double samples_per_beat, double num, int num_frames) {
  ResynchronizingParameterChanges result{};

  if (desc.is_float()) {
    //  .      |        .
    // prev  cursor    next
    auto frame_dist = int(pts.to_next.to_sample_offset(samples_per_beat, num));
    frame_dist = std::max(0, frame_dist);
    int frame_off{};

    const int immediate_target_dist = std::min(
      default_immediate_change_distance_samples(), num_frames - 1);
    const double tot_dist = std::max(
      1.0, pts.tot_distance.to_sample_offset(samples_per_beat, num));
    const int to_prev = int(
      std::max(0.0, pts.to_prev.to_sample_offset(samples_per_beat, num)));

    if (frame_dist > immediate_target_dist) {
      const double target_t = clamp01(double(immediate_target_dist + to_prev) / tot_dist);
      auto target_val = parameter_lerp(float(target_t), pts.prev->value, pts.next->value);
      result.changes[result.num_changes++] = make_audio_parameter_change(
        desc.ids, target_val, 0, immediate_target_dist);

      frame_dist -= immediate_target_dist;
      frame_off += immediate_target_dist;
    }

    result.changes[result.num_changes++] = make_audio_parameter_change(
      desc.ids, pts.next->value, frame_off, frame_dist);

  } else {
    result.changes[result.num_changes++] = make_immediate_change(desc.ids, pts.prev->value);
  }

  return result;
}

auto resynchronizing_changes(const GridPoints& pts, const AudioParameterDescriptor& desc, double num,
                             const BlockInfo& block_info, const AudioRenderInfo& render_info,
                             bool playing) {
  if (playing) {
    return resynchronizing_changes_playing(
      pts, desc, block_info.samples_per_beat, num, render_info.num_frames);
  } else {
    return resynchronizing_changes_stopped(pts, desc, block_info.samples_per_beat, num);
  }
}

bool has_change(const AudioParameterChanges& changes, int eval_size, AudioParameterIDs ids) {
  return AudioParameterChanges::find_first(
    changes.changes.data(),
    changes.changes.data() + eval_size,
    ids) != nullptr;
}

void push_change_event(AudioEventStreamHandle event_stream, const AudioParameterChange& change) {
  auto data = make_audio_event_data(change);
  auto evt = make_audio_event(AudioEvent::Type::NewAudioParameterValue, data);
  evt.frame = uint64_t(change.at_frame);
  (void) audio_event_system::render_push_event(event_stream, evt);
}

void resynchronize(InstanceData& inst, AudioParameterChanges& changes, const BreakPointSet* set,
                   const PartitionedBlock& partitioned_block, double num, bool playing,
                   AudioEventStreamHandle event_stream,
                   const BlockInfo& block_info, const AudioRenderInfo& info) {
  bool any_inserted{};
  const int num_changes = changes.size();
  for (AudioParameterIDs ids : inst.parameter_state_changes.need_resynchronize) {
    if (has_change(changes, num_changes, ids)) {
      continue;
    }
    ResynchronizingParameterChanges resync{};
    if (inst.is_ui_controlled(ids)) {
      resync.changes[resync.num_changes++] = make_immediate_change(
        ids, inst.parameter_instance.ui_values.at(ids));

    } else if (set) {
      if (auto it = set->find_parameter(ids); it != set->break_points.end()) {
        auto pts = surrounding_points(*it, partitioned_block.begin, set->span, num);
        resync = resynchronizing_changes(pts, it->descriptor, num, block_info, info, playing);
      }
    }

    for (int i = 0; i < resync.num_changes; i++) {
      changes.push(resync.changes[i]);
      push_change_event(event_stream, resync.changes[i]);
      any_inserted = true;
    }
  }
  if (any_inserted) {
    changes.sort();
  }
}

void revert_to_break_points(InstanceData& inst, AudioParameterChanges& changes,
                            const BreakPointSet& set, const PartitionedBlock& partitioned_block,
                            double num, bool playing, AudioEventStreamHandle event_stream,
                            const BlockInfo& block_info, const AudioRenderInfo& info) {
  bool any_inserted{};
  const int num_changes = changes.size();
  for (AudioParameterIDs ids : inst.parameter_state_changes.newly_reverted_to_break_points) {
    assert(!inst.is_ui_controlled(ids));
    if (has_change(changes, num_changes, ids)) {
      continue;
    }

    ResynchronizingParameterChanges resync{};
    if (auto it = set.find_parameter(ids); it != set.break_points.end()) {
      auto pts = surrounding_points(*it, partitioned_block.begin, set.span, num);
      resync = resynchronizing_changes(pts, it->descriptor, num, block_info, info, playing);
    }

    for (int i = 0; i < resync.num_changes; i++) {
      changes.push(resync.changes[i]);
      push_change_event(event_stream, resync.changes[i]);
      any_inserted = true;
    }
  }
  if (any_inserted) {
    changes.sort();
  }
}

void on_break_points_modified(InstanceData& inst, AudioParameterChanges& changes,
                              const BreakPointSet& set, const PartitionedBlock& partitioned_block,
                              double num, bool playing, AudioEventStreamHandle event_stream,
                              const BlockInfo& block_info, const AudioRenderInfo& info) {
  bool any_inserted{};
  const int num_changes = changes.size();
  for (auto& param : set.break_points) {
    if (inst.is_ui_controlled(param.descriptor.ids) ||
        has_change(changes, num_changes, param.descriptor.ids)) {
      continue;
    }
    auto pts = surrounding_points(param, partitioned_block.begin, set.span, num);
    auto resync = resynchronizing_changes(pts, param.descriptor, num, block_info, info, playing);
    for (int i = 0; i < resync.num_changes; i++) {
      changes.push(resync.changes[i]);
      push_change_event(event_stream, resync.changes[i]);
      any_inserted = true;
    }
  }
  if (any_inserted) {
    changes.sort();
  }
}

void emit_resynchronizing_events(InstanceData& inst, const AudioParameterChanges& changes,
                                 const BreakPointSet* set, const PartitionedBlock& partitioned_block,
                                 double num, bool playing, AudioEventStreamHandle event_stream,
                                 const BlockInfo& block_info, const AudioRenderInfo& info) {
  for (AudioParameterIDs ids : inst.parameter_instance.controlled_by_ui) {
    if (!has_change(changes, int(changes.changes.size()), ids)) {
      auto& val = inst.parameter_instance.ui_values.at(ids);
      push_change_event(event_stream, make_immediate_change(ids, val));
    }
  }
  if (set) {
    for (auto& param : set->break_points) {
      if (!inst.is_ui_controlled(param.descriptor.ids) &&
          !has_change(changes, changes.size(), param.descriptor.ids)) {
        auto pts = surrounding_points(param, partitioned_block.begin, set->span, num);
        auto resync = resynchronizing_changes(pts, param.descriptor, num, block_info, info, playing);
        for (int i = 0; i < resync.num_changes; i++) {
          push_change_event(event_stream, resync.changes[i]);
        }
      }
    }
  }
}

void process_break_points(const BreakPointSet* set, const BreakPointSet::BreakPointsByParameter& param,
                          AudioParameterChanges& changes, const PartitionedBlock& partitioned_block,
                          double num, const BlockInfo& block_info, const AudioRenderInfo& info,
                          AudioEventStreamHandle event_stream, bool bpm_changed) {
  const size_t num_points = param.points.size();
  const BreakPoint* beg = param.points.data();
  const BreakPoint* end = beg + num_points;
  int has_frame0_change{};

  for (int i = 0; i < partitioned_block.num_intervals; i++) {
    auto& interval = partitioned_block.intervals[i];
    const auto interval_end = interval.span.end(num);
    const BreakPoint* it = first_ge(beg, end, interval.span.begin);

    for (; it != end && it->position < interval_end; ++it) {
      auto& p0 = *it;

      auto p0_beg = p0.position;
      p0_beg.wrapped_sub_cursor(interval.span.begin, num);
      p0_beg.wrapped_add_cursor(interval.cumulative_offset, num);
      double frame_off = p0_beg.to_sample_offset(block_info.samples_per_beat, num);
      auto floor_off = int(frame_off);
      assert(floor_off >= 0 && floor_off < info.num_frames);
      floor_off = clamp(floor_off, 0, info.num_frames - 1);
      has_frame0_change = has_frame0_change | int(floor_off == 0);

      AudioParameterChange change;
      if (param.descriptor.is_float()) {
        auto& p1 = *next_it(beg, end, it);
        const int floor_dist = interpolating_frame_distance(
          p0, p1, set->span, num, block_info.samples_per_beat);
        change = make_audio_parameter_change(
          param.descriptor.ids, p1.value, floor_off, floor_dist);
      } else {
        change = make_audio_parameter_change(param.descriptor.ids, p0.value, floor_off, 0);
      }

      changes.push(change);
      push_change_event(event_stream, change);
    }
  }

  if (!has_frame0_change && bpm_changed && param.descriptor.is_float()) {
    auto pts = surrounding_points(beg, end, partitioned_block.begin, set->span, num);
    auto frame_dist = int(pts.to_next.to_sample_offset(block_info.samples_per_beat, num));
    frame_dist = std::max(0, frame_dist);
    auto change = make_audio_parameter_change(param.descriptor.ids, pts.next->value, 0, frame_dist);
    changes.push(change);
    push_change_event(event_stream, change);
  }
}

InstanceBreakPointSet* find_break_point_set(BreakPointInstanceData& inst,
                                            BreakPointSetHandle handle, int* index) {
  int ind{};
  for (auto& set : inst.break_point_sets) {
    if (set->handle == handle) {
      *index = ind;
      return set.get();
    }
    ind++;
  }
  *index = -1;
  return nullptr;
}

InstanceBreakPointSet* find_break_point_set(BreakPointInstanceData& inst,
                                            BreakPointSetHandle handle) {
  int ignore{};
  return find_break_point_set(inst, handle, &ignore);
}

[[maybe_unused]] bool is_valid_break_point(const BreakPoint& point) {
  return point.id > 0;
}

BreakPointSetModification make_remove_parent_modification(BreakPointSetHandle set,
                                                          AudioParameterID id) {
  BreakPointSetModification mod{};
  mod.type = BreakPointSetModification::Type::RemoveParent;
  mod.remove_parent = {};
  mod.remove_parent.set = set;
  mod.remove_parent.parent = id;
  return mod;
}

BreakPointSetModification
make_create_or_destroy_set_modification(BreakPointSetHandle set, const ScoreRegion& span, bool create) {
  BreakPointSetModification mod{};
  mod.type = create ?
    BreakPointSetModification::Type::CreateSet :
    BreakPointSetModification::Type::DestroySet;
  mod.create_or_destroy_set = {};
  mod.create_or_destroy_set.handle = set;
  mod.create_or_destroy_set.span = span;
  return mod;
}

BreakPointSetModification make_add_or_remove_point_modification(BreakPointSetHandle set,
                                                                const AudioParameterDescriptor& desc,
                                                                BreakPoint point,
                                                                bool add) {
  BreakPointSetModification mod{};
  mod.type = add ?
    BreakPointSetModification::Type::AddPoint :
    BreakPointSetModification::Type::RemovePoint;
  mod.add_or_remove_point = {};
  mod.add_or_remove_point.set = set;
  mod.add_or_remove_point.param_desc = desc;
  mod.add_or_remove_point.point = point;
  return mod;
}

BreakPointSetModification make_modify_point_modification(BreakPointSetHandle set,
                                                         const AudioParameterDescriptor& desc,
                                                         BreakPoint point) {
  BreakPointSetModification mod{};
  mod.type = BreakPointSetModification::Type::ModifyPoint;
  mod.modify_point = {};
  mod.modify_point.set = set;
  mod.modify_point.param_desc = desc;
  mod.modify_point.point = point;
  return mod;
}

void remove_parent(BreakPointInstanceData& inst, const BreakPointSetModification& mod) {
  auto& data = mod.remove_parent;
  auto* set = find_break_point_set(inst, data.set);
  assert(set);
  set->set.remove_matching_parent_id(data.parent);
}

void add_or_remove_point(BreakPointInstanceData& inst, const BreakPointSetModification& mod) {
  auto& data = mod.add_or_remove_point;
  auto* set = find_break_point_set(inst, data.set);
  assert(set);
  if (mod.type == BreakPointSetModification::Type::AddPoint) {
    set->set.add(data.param_desc, data.point);
  } else {
    assert(mod.type == BreakPointSetModification::Type::RemovePoint);
    set->set.remove(data.param_desc.ids, data.point);
  }
}

void modify_point(BreakPointInstanceData& inst, const BreakPointSetModification& mod) {
  auto& data = mod.modify_point;
  auto* set = find_break_point_set(inst, data.set);
  assert(set);
  if (!set->set.remove_matching_break_point_id(data.param_desc.ids, data.point.id)) {
    GROVE_LOG_WARNING_CAPTURE_META("No such break point id.", logging_id());
  }
  set->set.add(data.param_desc, data.point);
}

void create_or_destroy_set(BreakPointInstanceData& inst, const BreakPointSetModification& mod) {
  auto& data = mod.create_or_destroy_set;
  if (mod.type == BreakPointSetModification::Type::DestroySet) {
    int index{};
    auto* set = find_break_point_set(inst, data.handle, &index);
    assert(set && index >= 0);
    inst.break_point_sets.erase(inst.break_point_sets.begin() + index);
    if (inst.active_set == set) {
      inst.active_set = nullptr;
    }
  } else {
    assert(mod.type == BreakPointSetModification::Type::CreateSet);
    auto& set = inst.break_point_sets.emplace_back();
    set = std::make_unique<InstanceBreakPointSet>();
    set->handle = data.handle;
    set->set = make_break_point_set(data.span);
    if (!inst.active_set) {
      inst.active_set = set.get();
    }
  }
}

void apply_modification(BreakPointInstanceData& inst, const BreakPointSetModification& mod) {
  switch (mod.type) {
    case BreakPointSetModification::Type::AddPoint:
    case BreakPointSetModification::Type::RemovePoint:
      add_or_remove_point(inst, mod);
      break;
    case BreakPointSetModification::Type::ModifyPoint:
      modify_point(inst, mod);
      break;
    case BreakPointSetModification::Type::CreateSet:
    case BreakPointSetModification::Type::DestroySet:
      create_or_destroy_set(inst, mod);
      break;
    case BreakPointSetModification::Type::RemoveParent:
      remove_parent(inst, mod);
      break;
    default: {
      assert(false);
    }
  }
}

AudioParameterModification make_set_value_modification(AudioParameterIDs ids,
                                                       const AudioParameterValue& val) {
  AudioParameterModification mod{};
  mod.type = AudioParameterModification::Type::SetValue;
  mod.set_value = {};
  mod.set_value.ids = ids;
  mod.set_value.value = val;
  return mod;
}

AudioParameterModification make_revert_to_break_points_modification(AudioParameterIDs ids) {
  AudioParameterModification mod{};
  mod.type = AudioParameterModification::Type::RevertToBreakPoints;
  mod.revert_to_break_points = {};
  mod.revert_to_break_points.ids = ids;
  return mod;
}

AudioParameterModification make_remove_parent_modification(AudioParameterID id) {
  AudioParameterModification mod{};
  mod.type = AudioParameterModification::Type::RemoveParent;
  mod.remove_parent = {};
  mod.remove_parent.parent = id;
  return mod;
}

void remove_parent(ParameterInstanceData& inst, const AudioParameterModification& mod) {
  AudioParameterID id = mod.remove_parent.parent;
  remove_matching_parent(inst.ui_values, id);
  remove_matching_parent(inst.controlled_by_ui, id);
}

void set_value(ParameterInstanceData& inst, const AudioParameterModification& mod) {
  auto& data = mod.set_value;
  inst.ui_values[data.ids] = data.value;
  inst.controlled_by_ui.insert(data.ids);
}

void revert_to_break_points(ParameterInstanceData& inst, const AudioParameterModification& mod) {
  auto& data = mod.revert_to_break_points;
  inst.controlled_by_ui.erase(data.ids);
}

void apply_modification(ParameterInstanceData& inst, const AudioParameterModification& mod) {
  switch (mod.type) {
    case AudioParameterModification::Type::SetValue:
      set_value(inst, mod);
      break;
    case AudioParameterModification::Type::RevertToBreakPoints:
      revert_to_break_points(inst, mod);
      break;
    case AudioParameterModification::Type::RemoveParent:
      remove_parent(inst, mod);
      break;
    default: {
      assert(false);
    }
  }
}

template <typename I, typename T>
void apply_modifications(I& inst, const T& mods) {
  for (auto& mod : mods) {
    apply_modification(inst, mod);
  }
}

template <typename I, typename T>
void apply_push_modifications(I& inst, const T& src, T& dst) {
  for (auto& mod : src) {
    apply_modification(inst, mod);
    dst.push_back(mod);
  }
}

void ui_insert_resynchronizing(InstanceData& inst, const ArrayView<uint32_t>& connected_nodes) {
  for (uint32_t node : connected_nodes) {
    if (inst.break_point_instance.active_set) {
      for (auto& param : inst.break_point_instance.active_set->set.break_points) {
        if (param.descriptor.ids.parent == node) {
          inst.parameter_state_changes.need_resynchronize.insert(param.descriptor.ids);
        }
      }
    }
    for (AudioParameterIDs ids : inst.parameter_instance.controlled_by_ui) {
      if (ids.parent == node) {
        inst.parameter_state_changes.need_resynchronize.insert(ids);
      }
    }
  }
}

void ui_process_render_feedback(RenderData& from_render, ScoreCursor* dst_cursor) {
  const int num_pending = from_render.feedback_items.size();
  for (int i = 0; i < num_pending; i++) {
    auto item = from_render.feedback_items.read();
    switch (item.type) {
      case RenderFeedbackItem::Type::CursorLocation: {
        *dst_cursor = item.cursor_location.position;
        break;
      }
      default: {
        assert(false);
      }
    }
  }
}

} //  anon

struct AudioParameterSystem {
  bool set0_modified() const {
    return !set0.parameter_state_changes.empty() ||
           !bp_mods0.empty() ||
           !param_mods0.empty();
  }

  const Transport* transport{};
  AudioParameterWriteAccess parameter_write_access;
  AudioParameterWriterID self_writer_id{};

  InstanceData set0;
  InstanceData set1;
  InstanceData set2;

  InstanceData* dst0{};
  InstanceData* dst1{};
  InstanceData* render_instance{};
  Handshake<InstanceData*> instance_handshake;

  std::vector<BreakPointSetModification> bp_mods0;
  std::vector<BreakPointSetModification> bp_mods1;
  uint32_t next_break_point_set_id{1};

  std::vector<AudioParameterModification> param_mods0;
  std::vector<AudioParameterModification> param_mods1;

  ScoreCursor approx_active_set_cursor_position{};
  std::atomic<bool> ui_did_initialize{};
};

namespace {

struct {
  AudioParameterSystem sys;
} globals;

} //  anon

AudioParameterSystem* param_system::get_global_audio_parameter_system() {
  return &globals.sys;
}

void param_system::render_begin_process(AudioParameterSystem* sys, const AudioRenderInfo& info) {
  if (!sys->ui_did_initialize.load()) {
    return;
  }

  bool new_data{};
  bool points_modified{};
  if (auto inst = read(&sys->instance_handshake)) {
    sys->render_instance = inst.value();
    new_data = true;
    points_modified = sys->render_instance->break_points_modified;
  }

  auto& transport = *sys->transport;
  auto& inst = *sys->render_instance;

  auto& rd = *inst.render_data;
  auto& changes = rd.changes;
  changes.clear();

  {
    auto curs = transport.render_get_cursor_location();
    auto feedback = make_cursor_location_render_feedback_item(curs);
    render_maybe_push_feedback_item(rd, feedback);
  }

  const bool bpm_changed = transport.get_bpm() != rd.last_bpm;
  rd.last_bpm = transport.get_bpm();

  bool emit_events{true};
  rd.emit_events.compare_exchange_strong(emit_events, false);

  if (info.num_frames == 0) {
    return;
  }

  const auto event_stream = audio_event_system::default_event_stream();
  if (new_data) {
    for (auto& ids : inst.parameter_state_changes.newly_set_values) {
      assert(inst.is_ui_controlled(ids));
      auto change = make_immediate_change(ids, inst.parameter_instance.ui_values.at(ids));
      changes.push(change);
      push_change_event(event_stream, change);
    }
  }

  const auto block_info = get_block_info(transport, info.sample_rate, info.num_frames);
  const double num = block_info.tsig.numerator;
  const bool playing = transport.render_is_playing();

  PartitionedBlock partitioned_block{};
  const BreakPointSet* set{};

  if (auto* instance_set = inst.break_point_instance.active_set) {
    set = &instance_set->set;
    partition_block(&partitioned_block, set->span, block_info);
  }

  if (set && transport.just_stopped()) {
    for (auto& param : set->break_points) {
      if (!inst.is_ui_controlled(param.descriptor.ids)) {
        auto val = lerp(surrounding_points(param, partitioned_block.begin, set->span, num), num);
        auto change = make_immediate_change(param.descriptor.ids, val);
        changes.push(change);
        push_change_event(event_stream, change);
      }
    }
  } else if (set && playing) {
    for (auto& param : set->break_points) {
      if (!inst.is_ui_controlled(param.descriptor.ids)) {
        process_break_points(set, param, changes, partitioned_block,
                             num, block_info, info, event_stream, bpm_changed);
      }
    }
  }

  changes.sort();

  if (new_data) {
    if (!inst.parameter_state_changes.need_resynchronize.empty()) {
      resynchronize(inst, changes, set, partitioned_block,
                    num, playing, event_stream, block_info, info);
    }
    if (set && !inst.parameter_state_changes.newly_reverted_to_break_points.empty()) {
      revert_to_break_points(inst, changes, *set, partitioned_block,
                             num, playing, event_stream, block_info, info);
    }
  }

  if (points_modified && set) {
    on_break_points_modified(inst, changes, *set, partitioned_block,
                             num, playing, event_stream, block_info, info);
  }

  if (emit_events) {
    emit_resynchronizing_events(inst, changes, set, partitioned_block,
                                num, playing, event_stream, block_info, info);
  }
}

const AudioParameterChanges& param_system::render_read_changes(const AudioParameterSystem* sys) {
  return sys->render_instance->render_data->changes;
}

AudioParameterWriteAccess* param_system::ui_get_write_access(AudioParameterSystem* sys) {
  return &sys->parameter_write_access;
}

const AudioParameterWriteAccess* param_system::ui_get_write_access(const AudioParameterSystem* sys) {
  return &sys->parameter_write_access;
}

void param_system::ui_set_value(AudioParameterSystem* sys, AudioParameterWriterID writer,
                                AudioParameterIDs ids, const AudioParameterValue& value) {
  assert(sys->parameter_write_access.can_acquire(ids) ||
         sys->parameter_write_access.can_write(writer, ids));
  (void) writer;

  auto& inst = sys->set0;
  auto mod = make_set_value_modification(ids, value);
  apply_modification(inst.parameter_instance, mod);
  sys->param_mods0.push_back(mod);

  inst.parameter_state_changes.newly_reverted_to_break_points.erase(ids);
  inst.parameter_state_changes.newly_set_values.insert(ids);
}

bool param_system::ui_set_value_if_no_other_writer(
  AudioParameterSystem* sys, AudioParameterIDs ids, const AudioParameterValue& value) {
  //
  if (sys->parameter_write_access.request(sys->self_writer_id, ids)) {
    ui_set_value(sys, sys->self_writer_id, ids, value);
    sys->parameter_write_access.release(sys->self_writer_id, ids);
    return true;
  } else {
    return false;
  }
}

AudioParameterValue param_system::ui_get_set_value_or_default(const AudioParameterSystem* sys,
                                                              const AudioParameterDescriptor& desc) {
  auto& inst = sys->set0;
  auto it = inst.parameter_instance.ui_values.find(desc.ids);
  if (it != inst.parameter_instance.ui_values.end()) {
    return it->second;
  } else {
    return AudioParameterValue{desc.dflt, desc.type};
  }
}

void param_system::ui_remove_parent(AudioParameterSystem* sys, AudioParameterID id) {
  auto& inst = sys->set0;

  auto param_mod = make_remove_parent_modification(id);
  apply_modification(inst.parameter_instance, param_mod);
  sys->param_mods0.push_back(param_mod);

  for (auto& set : inst.break_point_instance.break_point_sets) {
    auto bp_mod = make_remove_parent_modification(set->handle, id);
    apply_modification(inst.break_point_instance, bp_mod);
    sys->bp_mods0.push_back(bp_mod);
  }

  inst.parameter_state_changes.remove_parent(id);
}

void param_system::ui_revert_to_break_points(AudioParameterSystem* sys,
                                             AudioParameterWriterID writer, AudioParameterIDs ids) {
  assert(sys->parameter_write_access.can_write(writer, ids));
  (void) writer;

  auto& inst = sys->set0;
  if (!inst.parameter_instance.controlled_by_ui.count(ids)) {
    return;
  }

  auto mod = make_revert_to_break_points_modification(ids);
  apply_modification(inst.parameter_instance, mod);
  sys->param_mods0.push_back(mod);

  inst.parameter_state_changes.newly_set_values.erase(ids);
  inst.parameter_state_changes.newly_reverted_to_break_points.insert(ids);
}

bool param_system::ui_is_ui_controlled(const AudioParameterSystem* sys, AudioParameterIDs ids) {
  auto& inst = sys->set0;
  return inst.parameter_instance.controlled_by_ui.count(ids) > 0;
}

bool param_system::ui_has_break_points(const AudioParameterSystem* sys, AudioParameterIDs ids) {
  for (auto& set : sys->set0.break_point_instance.break_point_sets) {
    if (set->set.has_parameter(ids)) {
      return true;
    }
  }
  return false;
}

void param_system::ui_end_update(AudioParameterSystem* sys,
                                 const AudioParameterSystemUpdateInfo& info) {
  if (info.any_dropped_events) {
    sys->render_instance->render_data->emit_events.store(true);
  }

  ui_insert_resynchronizing(sys->set0, info.connected_nodes);
  for (const uint32_t node : info.deleted_nodes) {
    ui_remove_parent(sys, node);
  }

  if (sys->instance_handshake.awaiting_read && acknowledged(&sys->instance_handshake)) {
    auto& inst = *sys->dst1;

    apply_modifications(inst.break_point_instance, sys->bp_mods1);
    sys->bp_mods1.clear();

    apply_modifications(inst.parameter_instance, sys->param_mods1);
    sys->param_mods1.clear();

    inst.break_points_modified = false;
    inst.parameter_state_changes.clear();
    std::swap(sys->dst0, sys->dst1);
  }

  if (sys->set0_modified() && !sys->instance_handshake.awaiting_read) {
    auto& dst = *sys->dst0;
    auto& src = sys->set0;
    assert(dst.parameter_state_changes.empty());

    assert(sys->bp_mods1.empty());
    const bool break_points_modified = !sys->bp_mods0.empty();
    apply_push_modifications(dst.break_point_instance, sys->bp_mods0, sys->bp_mods1);
    sys->bp_mods0.clear();

    assert(sys->param_mods1.empty());
    apply_push_modifications(dst.parameter_instance, sys->param_mods0, sys->param_mods1);
    sys->param_mods0.clear();

    dst.parameter_state_changes = std::move(src.parameter_state_changes);
    src.parameter_state_changes.clear();

    dst.break_points_modified = break_points_modified;
    publish(&sys->instance_handshake, std::move(sys->dst0));
  }

  ui_process_render_feedback(*sys->set0.render_data, &sys->approx_active_set_cursor_position);
  if (sys->set0.break_point_instance.active_set) {
    auto& set = sys->set0.break_point_instance.active_set->set;
    auto& curs = sys->approx_active_set_cursor_position;
    curs = set.span.loop(curs, reference_time_signature().numerator);
  }
}

void param_system::ui_initialize(AudioParameterSystem* sys, const Transport* transport) {
  assert(!sys->ui_did_initialize.load());
  sys->transport = transport;
  sys->self_writer_id = sys->parameter_write_access.create_writer();

  auto render_data = std::make_shared<RenderData>();
  render_data->changes.reserve_and_clear(1024);

  sys->set0.render_data = render_data;
  sys->set1.render_data = render_data;
  sys->set2.render_data = render_data;

  sys->dst0 = &sys->set1;
  sys->dst1 = &sys->set2;
  sys->render_instance = &sys->set2;

  sys->ui_did_initialize.store(true);
}

BreakPointSetHandle param_system::ui_create_break_point_set(AudioParameterSystem* sys,
                                                            const ScoreRegion& span) {
  assert(!span.empty());
  BreakPointSetHandle result{sys->next_break_point_set_id++};
  auto mod = make_create_or_destroy_set_modification(result, span, true);
  apply_modification(sys->set0.break_point_instance, mod);
  sys->bp_mods0.push_back(mod);
  return result;
}

void param_system::ui_destroy_break_point_set(AudioParameterSystem* sys,
                                              BreakPointSetHandle handle) {
  auto mod = make_create_or_destroy_set_modification(handle, {}, false);
  apply_modification(sys->set0.break_point_instance, mod);
  sys->bp_mods0.push_back(mod);
}

void param_system::ui_insert_break_point(AudioParameterSystem* sys, AudioParameterWriterID writer,
                                         BreakPointSetHandle set,
                                         const AudioParameterDescriptor& param_desc,
                                         const BreakPoint& point) {
  assert(is_valid_break_point(point));
  assert(sys->parameter_write_access.can_write(writer, param_desc.ids));
  (void) writer;

  auto mod = make_add_or_remove_point_modification(set, param_desc, point, true);
  apply_modification(sys->set0.break_point_instance, mod);
  sys->bp_mods0.push_back(mod);
}

void param_system::ui_remove_break_point(AudioParameterSystem* sys, AudioParameterWriterID writer,
                                         BreakPointSetHandle set,
                                         const AudioParameterDescriptor& param_desc,
                                         const BreakPoint& point) {
  assert(is_valid_break_point(point));
  assert(sys->parameter_write_access.can_write(writer, param_desc.ids));
  (void) writer;

  auto mod = make_add_or_remove_point_modification(set, param_desc, point, false);
  apply_modification(sys->set0.break_point_instance, mod);
  sys->bp_mods0.push_back(mod);
}

void param_system::ui_modify_break_point(AudioParameterSystem* sys, AudioParameterWriterID writer,
                                         BreakPointSetHandle set,
                                         const AudioParameterDescriptor& param_desc,
                                         const BreakPoint& point) {
  assert(is_valid_break_point(point));
  assert(sys->parameter_write_access.can_write(writer, param_desc.ids));
  (void) writer;

  auto mod = make_modify_point_modification(set, param_desc, point);
  apply_modification(sys->set0.break_point_instance, mod);
  sys->bp_mods0.push_back(mod);
}

const BreakPointSet* param_system::ui_read_break_point_set(const AudioParameterSystem* sys,
                                                           BreakPointSetHandle handle) {
  for (auto& set : sys->set0.break_point_instance.break_point_sets) {
    if (set->handle == handle) {
      return &set->set;
    }
  }
  return nullptr;
}

Optional<BreakPointSetHandle>
param_system::ui_get_active_set_handle(const AudioParameterSystem* sys) {
  if (auto* set = sys->set0.break_point_instance.active_set) {
    return Optional<BreakPointSetHandle>(set->handle);
  } else {
    return NullOpt{};
  }
}

ScoreCursor
param_system::ui_get_active_break_point_set_cursor_position(const AudioParameterSystem* sys) {
  return sys->approx_active_set_cursor_position;
}

param_system::Stats param_system::ui_get_stats(const AudioParameterSystem* sys) {
  auto& inst = sys->set0;

  Stats stats{};
  stats.num_newly_set_values = int(inst.parameter_state_changes.newly_set_values.size());
  stats.num_newly_reverted_to_break_points = int(
    inst.parameter_state_changes.newly_reverted_to_break_points.size());
  stats.num_need_resynchronize = int(inst.parameter_state_changes.need_resynchronize.size());
  stats.num_break_point_sets = int(inst.break_point_instance.break_point_sets.size());

  for (auto& set : inst.break_point_instance.break_point_sets) {
    auto& bps = set->set.break_points;
    stats.num_break_point_parameters += int(bps.size());
    for (auto& param : bps) {
      stats.total_num_break_points += int(param.points.size());
    }
  }

  stats.num_ui_values = int(inst.parameter_instance.ui_values.size());
  stats.num_controlled_by_ui = int(inst.parameter_instance.controlled_by_ui.size());
  stats.num_write_access_acquired_parameters = sys->parameter_write_access.num_in_use();
  return stats;
}

GROVE_NAMESPACE_END
