#include "audio_parameters.hpp"
#include "grove/common/common.hpp"
#include "grove/common/vector_util.hpp"
#include "grove/common/logging.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

template <typename T>
auto lower_bound_on_parameter_ids(T&& break_points, AudioParameterIDs ids) {
  return std::lower_bound(break_points.begin(), break_points.end(), ids, [](auto& a, auto& b) {
    return a.descriptor.ids < b;
  });
}

template <typename T>
auto lower_bound_on_parent(T&& break_points, AudioParameterID id) {
  return std::lower_bound(break_points.begin(), break_points.end(), id, [](auto& a, auto& b) {
    return a.descriptor.ids.parent < b;
  });
}

template <typename T>
auto find_by_break_point_id(T&& break_points, BreakPointID id) {
  return std::find_if(break_points.begin(), break_points.end(), [id](auto& bp) {
    return bp.id == id;
  });
}

} //  anon

/*
 * AudioParameterDescriptor
 */

float AudioParameterDescriptor::linear_frac_range(const AudioParameterValue& value) const {
  switch (value.type) {
    case AudioParameterType::Float: {
      assert(is_float());
      return clamp((value.data.f - min.f) / (max.f - min.f), 0.0f, 1.0f);
    }
    case AudioParameterType::Int: {
      assert(is_int());
      auto v = float(value.data.i);
      return clamp((v - float(min.i)) / (float(max.i) - float(min.i)), 0.0f, 1.0f);
    }
    default: {
      assert(false);
      return 0.0f;
    }
  }
}

/*
 * AudioParameterValue
 */

bool operator==(const AudioParameterValue& a, const AudioParameterValue& b) {
  if (a.type != b.type) {
    return false;
  }

  switch (a.type) {
    case AudioParameterType::Float:
      return a.data.f == b.data.f;
    case AudioParameterType::Int:
      return a.data.i == b.data.i;
    default:
      assert(false);
      return false;
  }
}

bool operator!=(const AudioParameterValue& a, const AudioParameterValue& b) {
  return !(a == b);
}

void AudioParameterValue::assign(int v) {
  data.i = v;
  type = AudioParameterType::Int;
}

void AudioParameterValue::assign(float v) {
  data.f = v;
  type = AudioParameterType::Float;
}

float AudioParameterValue::to_float01(const AudioParameterData& mn,
                                      const AudioParameterData& mx) const {
  if (is_float()) {
    return clamp01((data.f - mn.f) / (mx.f - mn.f));

  } else if (is_int()) {
    const auto v = float(data.i);
    const auto mnf = float(mn.i);
    const auto mxf = float(mx.i);
    return clamp01((v - mnf) / (mxf - mnf));

  } else {
    assert(false && "Unhandled.");
    return 0.0f;
  }
}

AudioParameterValue parameter_lerp(float t,
                                   const AudioParameterValue& a,
                                   const AudioParameterValue& b) {
  assert(a.type == b.type);
  auto res = a;

  switch (res.type) {
    case AudioParameterType::Float:
      res.data.f = lerp(t, a.data.f, b.data.f);
      break;
    case AudioParameterType::Int: {
      res.data.i = rounded_integer_lerp(t, a.data.i, b.data.i);
      break;
    }
    default:
      assert(false);
  }

  return res;
}

AudioParameterValue make_float_parameter_value(float value) {
  AudioParameterValue val;
  val.assign(value);
  return val;
}

AudioParameterValue make_int_parameter_value(int value) {
  AudioParameterValue val;
  val.assign(value);
  return val;
}

bool lies_within_closed_range(const AudioParameterValue& v, const AudioParameterData& min,
                              const AudioParameterData& max) {
  switch (v.type) {
    case AudioParameterType::Float:
      return v.data.f >= min.f && v.data.f <= max.f;
    case AudioParameterType::Int:
      return v.data.i >= min.i && v.data.i <= max.i;
    default: {
      assert(false);
      return false;
    }
  }
}

AudioParameterValue
make_interpolated_parameter_value_from_descriptor(const AudioParameterDescriptor& descriptor, float t) {
  assert(t >= 0.0f && t <= 1.0f);

  AudioParameterValue min_value{};
  AudioParameterValue max_value{};

  switch (descriptor.type) {
    case AudioParameterType::Int: {
      min_value = make_int_parameter_value(descriptor.min.i);
      max_value = make_int_parameter_value(descriptor.max.i);
      break;
    }
    case AudioParameterType::Float: {
      min_value = make_float_parameter_value(descriptor.min.f);
      max_value = make_float_parameter_value(descriptor.max.f);
      break;
    }
    default:
      assert(false);
  }

  return parameter_lerp(t, min_value, max_value);
}

AudioParameterValue
make_min_parameter_value_from_descriptor(const AudioParameterDescriptor& descriptor) {
  return make_interpolated_parameter_value_from_descriptor(descriptor, 0.0f);
}

/*
 * BreakPoint
 */

std::atomic<BreakPointID> BreakPoint::next_break_point_id{1};

BreakPoint make_break_point(AudioParameterValue value, ScoreCursor position) {
  BreakPoint result{};
  result.value = value;
  result.position = position;
  result.id = BreakPoint::next_break_point_id++;
  return result;
}

const BreakPoint* first_ge(const BreakPoint* beg, const BreakPoint* end, const ScoreCursor& cursor) {
  return std::lower_bound(beg, end, cursor, [](auto& a, auto& b) {
    return a.position < b;
  });
}

const BreakPoint* first_gt(const BreakPoint* beg, const BreakPoint* end, const ScoreCursor& cursor) {
  return std::upper_bound(beg, end, cursor, [](auto& a, auto& b) {
    return a < b.position;
  });
}

/*
 * BreakPointSet
 */

BreakPointSet make_break_point_set(const ScoreRegion& span) {
  BreakPointSet result{};
  result.span = span;
  result.time_signature = reference_time_signature();
  return result;
}

BreakPointSet::BreakPointSet() :
 cursor{},
 span{{}, {4, 0.0}},
 time_signature(4, 4) {
  //
}

BreakPointSet::BreakPointsIter BreakPointSet::find_parameter(const AudioParameterIDs& ids) {
  auto it = lower_bound_on_parameter_ids(break_points, ids);
  return it == break_points.end() || it->descriptor.ids != ids ? break_points.end() : it;
}

BreakPointSet::BreakPointsConstIter BreakPointSet::find_parameter(const AudioParameterIDs& ids) const {
  auto it = lower_bound_on_parameter_ids(break_points, ids);
  return it == break_points.end() || it->descriptor.ids != ids ? break_points.end() : it;
}

bool BreakPointSet::has_parameter(const AudioParameterIDs& ids) const {
  return find_parameter(ids) != break_points.end();
}

void BreakPointSet::add(const AudioParameterDescriptor& descriptor, BreakPoint point) {
  assert(point.id > 0);

  auto parent_it = lower_bound_on_parameter_ids(break_points, descriptor.ids);
  point.position = span.keep_within(point.position, time_signature.numerator);

  if (parent_it == break_points.end() ||
      parent_it->descriptor.ids != descriptor.ids) {
    //  Create a new array of break points, because no other points are present with this point's
    //  descriptor.
    auto pos = parent_it - break_points.begin();
    break_points.insert(parent_it, {descriptor, {}});
    break_points[pos].points.push_back(point);

  } else {
    //  We already have some break points for this descriptor.
    auto& parent_points = parent_it->points;
    assert(find_by_break_point_id(parent_points, point.id) == parent_points.end());

    auto insert_it = std::lower_bound(
      parent_points.begin(), parent_points.end(), point, [](auto& a, auto& b) {
        return a.position < b.position;
      });

    //  If there's an existing point with the same exact position as the incoming point, overwrite
    //  it with the incoming point; otherwise, insert the incoming point.
    if (insert_it != parent_points.end() &&
        insert_it->position == point.position) {
      *insert_it = point;
    } else {
      parent_points.insert(insert_it, point);
    }
  }
}

bool BreakPointSet::remove_matching_parent_id(AudioParameterID id) {
  auto parent_begin = lower_bound_on_parent(break_points, id);
  auto it = parent_begin;
  while (it != break_points.end() && it->descriptor.ids.parent == id) {
    ++it;
  }
  break_points.erase(parent_begin, it);
  return it != parent_begin;
}

void BreakPointSet::remove_matching_parameter(const AudioParameterIDs& ids) {
  auto it = find_parameter(ids);
  if (it == break_points.end()) {
    GROVE_LOG_WARNING_CAPTURE_META("No such break point.", "BreakPointSet");
  } else {
    break_points.erase(it);
  }
}

bool BreakPointSet::remove_matching_break_point_id(const AudioParameterIDs& param_ids,
                                                   BreakPointID id) {
  auto remove_at = find_parameter(param_ids);
  if (remove_at == break_points.end()) {
    return false;
  }

  auto it = find_by_break_point_id(remove_at->points, id);
  if (it == remove_at->points.end()) {
    return false;
  }

  remove_at->points.erase(it);
  if (remove_at->points.empty()) {
    break_points.erase(remove_at);
  }

  return true;
}

bool BreakPointSet::remove(const AudioParameterIDs& ids, const BreakPoint& point) {
  auto remove_at = find_parameter(ids);
  if (remove_at == break_points.end()) {
    return false;
  }

  const auto point_begin = remove_at->points.begin();
  const auto point_end = remove_at->points.end();
  auto maybe_point = std::find(point_begin, point_end, point);

  if (maybe_point == point_end) {
    return false;
  }

  remove_at->points.erase(maybe_point);
  if (remove_at->points.empty()) {
    break_points.erase(remove_at);
  }

  return true;
}

void BreakPointSet::increment_cursor(double beats) {
  cursor.wrapped_add_beats(beats, time_signature.numerator);
  cursor = span.keep_within(cursor, time_signature.numerator);
}

int BreakPointSet::num_points(const AudioParameterIDs& ids) const {
  auto maybe_param = find_parameter(ids);
  if (maybe_param == break_points.end()) {
    return 0;
  } else {
    return int(maybe_param->points.size());
  }
}

int BreakPointSet::num_parameters() const {
  return int(break_points.size());
}

/*
 * AudioParameterChanges
 */

AudioParameterChangeView
AudioParameterChangeView::view_by_parameter(AudioParameterID param_id, int64_t offset) const {
  auto beg = begin + offset;
  return AudioParameterChanges::view_by_parameter(beg, end, param_id);
}

bool AudioParameterChangeView::collapse_to_last_change(AudioParameterChange* result) const noexcept {
  if (size() == 0) {
    return false;
  }

  auto first = *begin;
  const auto& last = *(end-1);

  first.value = last.value;
  //  We intend to initiate the change at frame 0, so the change will take longer to actually
  //  complete.
  first.frame_distance_to_target = last.at_frame + last.frame_distance_to_target;

  *result = first;
  return true;
}

void AudioParameterChanges::sort() {
  std::sort(changes.begin(), changes.end(), [](auto& a, auto& b) {
    return a.ids < b.ids ||
           (a.ids == b.ids && a.at_frame < b.at_frame);
  });
}

void AudioParameterChanges::clear() {
  changes.clear();
}

void AudioParameterChanges::reserve_and_clear(int count) {
  if (int(changes.capacity()) < count) {
    changes.resize(count);
    changes.clear();
  }
}

//  Get a view of the list of parameter changes (which might be empty) for this parent.
AudioParameterChangeView AudioParameterChanges::view_by_parent(AudioParameterID id) const {
  const auto change_beg = changes.begin();
  const auto change_end = changes.end();

  return AudioParameterChanges::view_by_parent(change_beg, change_end, id);
}

const AudioParameterChange* AudioParameterChanges::find_first(const AudioParameterChange* beg,
                                                              const AudioParameterChange* end,
                                                              AudioParameterIDs ids) {
  auto it = std::lower_bound(beg, end, ids, [](auto& a, const AudioParameterIDs& b) {
    return a.ids < b;
  });
  return it == end || it->ids != ids ? nullptr : it;
}

AudioParameterChangeView
AudioParameterChanges::view_by_parent(const AudioParameterChange* begin,
                                      const AudioParameterChange* end,
                                      AudioParameterID id) {
  auto maybe_lb =
    std::lower_bound(begin, end, id, [](auto& a, const AudioParameterID& b) {
      return a.ids.parent < b;
    });

  int64_t ind_beg = maybe_lb - begin;
  int64_t ind_end = ind_beg;
  const int64_t num_changes = end - begin;

  while (ind_end < num_changes && begin[ind_end].ids.parent == id) {
    ind_end++;
  }

  return {begin + ind_beg, begin + ind_end};
}

AudioParameterChangeView
AudioParameterChanges::view_by_parameter(const AudioParameterChange* begin,
                                         const AudioParameterChange* end,
                                         AudioParameterID param_id) {
  auto param_begin = std::find_if(begin, end, [param_id](const auto& param) {
    return param.ids.self == param_id;
  });

  int64_t ind_beg = param_begin - begin;
  int64_t ind_end = ind_beg;
  const int64_t num_changes = end - begin;

  while (ind_end < num_changes && begin[ind_end].ids.self == param_id) {
    ind_end++;
  }

  return {begin + ind_beg, begin + ind_end};
}

GROVE_NAMESPACE_END
