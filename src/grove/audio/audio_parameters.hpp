#pragma once

#include "types.hpp"
#include "grove/math/util.hpp"
#include <vector>
#include <algorithm>
#include <atomic>
#include <string_view>

namespace grove {

struct AudioParameterValue;
using AudioParameterID = uint32_t;

constexpr AudioParameterID null_audio_parameter_id() {
  return 0;
}

/*
 * AudioParameterIDs
 */

struct AudioParameterIDs {
public:
  using Self = AudioParameterIDs;

  struct Hash {
    inline std::size_t operator()(const Self& a) const noexcept {
      return std::hash<uint64_t>{}((uint64_t(a.parent) << 32u) | uint64_t(a.self));
    }
  };

  friend bool operator==(const Self& a, const Self& b);
  friend bool operator!=(const Self& a, const Self& b);
  friend bool operator<(const Self& a, const Self& b);
  friend bool operator<=(const Self& a, const Self& b);
  friend bool operator>(const Self& a, const Self& b);
  friend bool operator>=(const Self& a, const Self& b);

public:
  AudioParameterID parent;
  AudioParameterID self;
};

constexpr AudioParameterIDs null_audio_parameter_ids() {
  return {0, 0};
}

inline bool operator==(const AudioParameterIDs& a, const AudioParameterIDs& b) {
  return a.parent == b.parent && a.self == b.self;
}

inline bool operator!=(const AudioParameterIDs& a, const AudioParameterIDs& b) {
  return !(a == b);
}

inline bool operator<(const AudioParameterIDs& a, const AudioParameterIDs& b) {
  return a.parent < b.parent || (a.parent == b.parent && a.self < b.self);
}

inline bool operator>(const AudioParameterIDs& a, const AudioParameterIDs& b) {
  return a.parent > b.parent || (a.parent == b.parent && a.self > b.self);
}

inline bool operator>=(const AudioParameterIDs& a, const AudioParameterIDs& b) {
  return !(a < b);
}

inline bool operator<=(const AudioParameterIDs& a, const AudioParameterIDs& b) {
  return !(a > b);
}

/*
 * AudioParameterType
 */

enum class AudioParameterType {
  Float,
  Int
};

template <typename T> struct WhichAudioParameterType {};

template <> struct WhichAudioParameterType<float> {
  static constexpr AudioParameterType type = AudioParameterType::Float;
};

template <> struct WhichAudioParameterType<int> {
  static constexpr AudioParameterType type = AudioParameterType::Int;
};

template <typename T> struct IsAudioParameterType : public std::false_type{};
template <> struct IsAudioParameterType<float> : public std::true_type{};
template <> struct IsAudioParameterType<int> : public std::true_type{};

constexpr double default_immediate_change_distance_seconds() {
  return 10e-3;
}
//  When a parameter is to be changed "immediately", over how many samples should we ramp towards
//  the true value?
constexpr int default_immediate_change_distance_samples() {
  //  Tuned for likely sample rate.
  return int(44.1e3 * default_immediate_change_distance_seconds());
}

/*
 * AudioParameterData
 */

union AudioParameterData {
  float f;
  int i;
};

/*
 * AudioParameterDescriptor
 */

struct AudioParameterDescriptor {
  using Flag = uint32_t;

  struct Flags {
    static constexpr Flag non_editable = Flag(1);
    static constexpr Flag monitorable = Flag(Flag(1) << Flag(1));

    bool is_editable() const {
      return !(data & non_editable);
    }
    bool is_monitorable() const {
      return data & monitorable;
    }
    void mark_non_editable() {
      data |= non_editable;
    }
    void mark_monitorable() {
      data |= monitorable;
    }

    static inline Flags marked_monitorable_non_editable() {
      Flags res{};
      res.mark_non_editable();
      res.mark_monitorable();
      return res;
    }

    Flag data;
  };

public:
  bool is_float() const {
    return type == AudioParameterType::Float;
  }
  bool is_int() const {
    return type == AudioParameterType::Int;
  }
  bool is_editable() const {
    return flags.is_editable();
  }
  bool is_monitorable() const {
    return flags.is_monitorable();
  }
  bool matches_name(std::string_view query) const {
    return std::string_view{name} == query;
  }

  float linear_frac_range(const AudioParameterValue& value) const;

public:
  AudioParameterIDs ids;
  AudioParameterType type;
  AudioParameterData dflt;
  AudioParameterData min;
  AudioParameterData max;
  const char* name;
  Flags flags;
};

using AudioParameterDescriptors = DynamicArray<AudioParameterDescriptor, 16>;
using AudioParameterDescriptorPtrs = DynamicArray<const AudioParameterDescriptor*, 16>;

namespace detail {
  template <typename T>
  struct DescriptorVisitor {};

  template <>
  struct DescriptorVisitor<float> {
    static void assign(AudioParameterDescriptor& descriptor, float dflt, float min, float max) {
      descriptor.dflt.f = dflt;
      descriptor.min.f = min;
      descriptor.max.f = max;
    }
  };

  template <>
  struct DescriptorVisitor<int> {
    static void assign(AudioParameterDescriptor& descriptor, int dflt, int min, int max) {
      descriptor.dflt.i = dflt;
      descriptor.min.i = min;
      descriptor.max.i = max;
    }
  };
}

template <typename T, typename Func>
auto filter_audio_parameter_descriptors(const T& descriptors, Func&& filter) {
  AudioParameterDescriptorPtrs result;
  for (auto& descriptor : descriptors) {
    if (filter(descriptor)) {
      if constexpr (std::is_same_v<decltype(descriptor), const AudioParameterDescriptor&>) {
        result.push_back(&descriptor);
      } else {
        result.push_back(descriptor);
      }
    }
  }
  return result;
}

template <typename T>
auto only_monitorable_descriptors(const T& descriptors) {
  return filter_audio_parameter_descriptors(descriptors, [](auto&& descr) {
    return descr.is_monitorable();
  });
}

template <typename T>
AudioParameterDescriptor
make_audio_parameter_descriptor(const AudioParameterIDs& ids, T dflt, T min, T max, const char* name,
                                const AudioParameterDescriptor::Flags& flags = AudioParameterDescriptor::Flags{0}) {
  static_assert(IsAudioParameterType<T>::value, "Argument type is not a valid parameter type.");

  AudioParameterDescriptor descriptor{};
  descriptor.ids = ids;
  descriptor.type = WhichAudioParameterType<T>::type;
  detail::DescriptorVisitor<T>::assign(descriptor, dflt, min, max);
  descriptor.name = name;
  descriptor.flags = flags;

  return descriptor;
}

/*
 * AudioParameterValue
 */

struct AudioParameterValue {
public:
  using Self = AudioParameterValue;

  friend bool operator==(const Self& a, const Self& b);
  friend bool operator!=(const Self& a, const Self& b);

  void assign(float value);
  void assign(int value);

  bool is_float() const {
    return type == AudioParameterType::Float;
  }
  bool is_int() const {
    return type == AudioParameterType::Int;
  }

  float to_float01(const AudioParameterData& mn, const AudioParameterData& mx) const;

public:
  AudioParameterData data;
  AudioParameterType type;
};

//  Interpolate between audio parameter values from the same parameter source.
AudioParameterValue parameter_lerp(float t,
                                   const AudioParameterValue& a,
                                   const AudioParameterValue& b);

AudioParameterValue make_float_parameter_value(float value);
AudioParameterValue make_int_parameter_value(int value);
bool lies_within_closed_range(const AudioParameterValue& v, const AudioParameterData& min,
                              const AudioParameterData& max);

//  Return an AudioParameterValue whose underlying data is an interpolated value between the
//  parameter descriptor's min and max values, based on the [0, 1) float value `t`.
AudioParameterValue
make_interpolated_parameter_value_from_descriptor(const AudioParameterDescriptor& descriptor, float t);

//  Return an AudioParameterValue whose underlying data is the parameter descriptor's min value.
AudioParameterValue make_min_parameter_value_from_descriptor(const AudioParameterDescriptor& descriptor);

/*
 * BreakPoint
 */

using BreakPointID = uint32_t;

struct BreakPoint {
public:
  static std::atomic<BreakPointID> next_break_point_id;

public:
  friend inline bool operator==(const BreakPoint& a, const BreakPoint& b) {
    return a.value == b.value && a.position == b.position && a.id == b.id;
  }
  friend inline bool operator!=(const BreakPoint& a, const BreakPoint& b) {
    return !(a == b);
  }

public:
  AudioParameterValue value;
  ScoreCursor position;
  BreakPointID id;
};

BreakPoint make_break_point(AudioParameterValue value, ScoreCursor position);
const BreakPoint* first_ge(const BreakPoint* beg, const BreakPoint* end, const ScoreCursor& cursor);
const BreakPoint* first_gt(const BreakPoint* beg, const BreakPoint* end, const ScoreCursor& cursor);

/*
 * BreakPointSet
 */

class BreakPointSet {
  friend class BreakPointSetTest;

public:
  struct BreakPointsByParameter {
    AudioParameterDescriptor descriptor;
    std::vector<BreakPoint> points;
  };

  using BreakPointsIter = std::vector<BreakPointsByParameter>::iterator;
  using BreakPointsConstIter = std::vector<BreakPointsByParameter>::const_iterator;

public:
  BreakPointSet();

  void add(const AudioParameterDescriptor& descriptor, BreakPoint point);
  bool remove(const AudioParameterIDs& ids, const BreakPoint& point);
  bool remove_matching_break_point_id(const AudioParameterIDs& param_ids, BreakPointID id);
  void remove_matching_parameter(const AudioParameterIDs& ids);
  bool remove_matching_parent_id(AudioParameterID id);

  int num_points(const AudioParameterIDs& for_ids) const;
  int num_parameters() const;

public:
  void increment_cursor(double beats);
  BreakPointsIter find_parameter(const AudioParameterIDs& ids);
  BreakPointsConstIter find_parameter(const AudioParameterIDs& ids) const;
  bool has_parameter(const AudioParameterIDs& ids) const;

public:
  ScoreCursor cursor;
  ScoreRegion span;
  TimeSignature time_signature;

  std::vector<BreakPointsByParameter> break_points;
};

BreakPointSet make_break_point_set(const ScoreRegion& span);

struct AudioParameterChange {
  AudioParameterIDs ids;
  AudioParameterValue value;
  int at_frame;
  int frame_distance_to_target;
};

inline AudioParameterChange make_audio_parameter_change(const AudioParameterIDs& ids,
                                                        const AudioParameterValue& target,
                                                        int at_frame,
                                                        int frame_distance) {
  return {ids, target, at_frame, frame_distance};
}

struct AudioParameterChangeView {
public:
  inline int64_t size() const noexcept {
    return end - begin;
  }

  inline bool empty() const noexcept {
    return size() == 0;
  }

  inline bool should_change_now(int change_index, int frame) const noexcept {
    return change_index < (end - begin) && begin[change_index].at_frame == frame;
  }

  inline const AudioParameterChange& operator[](int index) const noexcept {
    assert(index >= 0 && index < size());
    return begin[index];
  }

  AudioParameterChangeView view_by_parameter(AudioParameterID param_id, int64_t offset = 0) const;

  //  If multiple changes are scheduled for a given parameter id in a single render epoch,
  //  keep only the latest one, and adjust the number of frames required to reach the target by
  //  assuming that the latest parameter change will begin at frame 0 of the render epoch.
  bool collapse_to_last_change(AudioParameterChange* result) const noexcept;

public:
  const AudioParameterChange* begin;
  const AudioParameterChange* end;
};

struct AudioParameterChanges {
public:
  AudioParameterChanges() = default;
  AudioParameterChanges(const AudioParameterChanges&) = delete;
  AudioParameterChanges& operator=(const AudioParameterChanges&) = delete;
  AudioParameterChanges(AudioParameterChanges&&) = delete;
  AudioParameterChanges& operator=(AudioParameterChanges&&) = delete;

  //  Order changes such that they are grouped first by parent ID, then by parameter ID, then by
  //  frame index.
  void sort();

  //  Clear changes.
  void clear();

  void push(const AudioParameterChange& change) {
    changes.push_back(change);
  }

  void reserve_and_clear(int count);
  int size() const {
    return int(changes.size());
  }

  //  Get a view of the list of parameter changes (which might be empty) for this parent.
  AudioParameterChangeView view_by_parent(AudioParameterID id) const;

  static AudioParameterChangeView view_by_parent(const AudioParameterChange* begin,
                                                 const AudioParameterChange* end,
                                                 AudioParameterID id);

  static AudioParameterChangeView view_by_parameter(const AudioParameterChange* begin,
                                                    const AudioParameterChange* end,
                                                    AudioParameterID param_id);

  static const AudioParameterChange* find_first(const AudioParameterChange* begin,
                                                const AudioParameterChange* end,
                                                AudioParameterIDs ids);

public:
  DynamicArray<AudioParameterChange, 64> changes;
};

/*
 * AudioParameterChangeTraits
 */

template <typename T, typename Limits>
struct AudioParameterChangeTraits {};

/*
 * ParameterLimits
 */

template <typename T>
struct DynamicParameterLimits {
public:
  T minimum() const;
  T maximum() const;
  T span() const;

public:
  T min{};
  T max{};
};

template <typename T>
T DynamicParameterLimits<T>::minimum() const {
  return min;
}

template <typename T>
T DynamicParameterLimits<T>::maximum() const {
  return max;
}

template <typename T>
T DynamicParameterLimits<T>::span() const {
  return max - min;
}

template <typename T>
struct StaticLimits01 {
  T minimum() const {
    return min;
  }
  T maximum() const {
    return max;
  }

public:
  static constexpr T min{0};
  static constexpr T max{1};
};

template <typename T>
struct StaticLimits11 {
  T minimum() const {
    return min;
  }
  T maximum() const {
    return max;
  }

public:
  static constexpr T min{-1};
  static constexpr T max{1};
};

template <int Min, int Max>
struct StaticIntLimits {
  int minimum() const {
    return Min;
  }
  int maximum() const {
    return Max;
  }
};

#define GROVE_DECLARE_CONSTEXPR_FLOAT_LIMITS(name, min_, max_)  \
  struct name {                                                 \
    static constexpr float min = (min_);                        \
    static constexpr float max = (max_);                        \
    float minimum() const {                                     \
      return min;                                               \
    }                                                           \
    float maximum() const {                                     \
      return max;                                               \
    }                                                           \
  }                                                             \

/*
 * AudioParameter
 */

template <typename T,
          typename Limits = DynamicParameterLimits<T>,
          typename Traits = AudioParameterChangeTraits<T, Limits>>
struct AudioParameter {
  using Flags = AudioParameterDescriptor::Flags;

  AudioParameter() = default;

  template <typename U, typename... LimitArgs>
  explicit AudioParameter(U&& val, LimitArgs&&... args) :
    value{std::forward<U>(val)},
    target{value},
    limits{std::forward<LimitArgs>(args)...} {
    static_assert(IsAudioParameterType<T>::value, "Type is not a valid AudioParameter type.");
  }

  void jump_to_target() {
    value = target;
    remaining = 0;
  }

  void set_from_fraction(T val) {
    set(grove::lerp(val, limits.minimum(), limits.maximum()));
  }

  void set(T val) {
    value = clamp(val);
  }

  auto clamp(T val) const {
    return grove::clamp(val, limits.minimum(), limits.maximum());
  }

  void apply(const AudioParameterChange& change) noexcept {
    Traits::apply(*this, change);
  }

  T evaluate() noexcept {
    return Traits::evaluate(*this);
  }

  bool change_complete() const noexcept {
    return target == value;
  }

  AudioParameterDescriptor make_default_descriptor(
    AudioParameterID parent_id, AudioParameterID self_id, const char* name,
    const Flags& flags = Flags{0}) const {
    //
    return make_descriptor(parent_id, self_id, value, name, flags);
  }

  AudioParameterDescriptor make_descriptor(AudioParameterID parent_id, AudioParameterID self_id,
                                           T dflt, const char* name,
                                           const Flags& flags = Flags{0}) const {
    AudioParameterIDs ids{parent_id, self_id};
    return make_audio_parameter_descriptor(
      ids, dflt, limits.minimum(), limits.maximum(), name, flags);
  }

  T value{};
  T target{};
  int remaining{};
  Limits limits{};
};

//  Check whether there's a new parameter change to apply on the given frame `frame_index`.
//  If there is, apply it to the parameter `param`, and increment the `next_change_index`.
template <typename Parameter>
inline void maybe_apply_change(const AudioParameterChangeView& view,
                               int& next_change_index,
                               Parameter& param,
                               int frame_index) {
  if (view.should_change_now(next_change_index, frame_index)) {
    param.apply(view[next_change_index++]);
  }
}

/*
 * AudioParameterChangeTraits
 */

template <typename Limits>
struct AudioParameterChangeTraits<float, Limits> {
  using Parameter =
    AudioParameter<float, Limits, AudioParameterChangeTraits<float, Limits>>;

  static void apply(Parameter& p, const AudioParameterChange& change) noexcept {
    assert(change.value.is_float());
    p.target = change.value.data.f;
    p.remaining = change.frame_distance_to_target <= 0 ?
                    default_immediate_change_distance_samples() :
                    change.frame_distance_to_target;
  }

  static float evaluate(Parameter& p) noexcept {
    if (p.remaining > 0) {
      auto delta = (p.target - p.value) / float(p.remaining);
      p.value = clamp(p.value + delta, p.limits.minimum(), p.limits.maximum());
      p.remaining--;
    }

    return p.value;
  }
};

/*
 * Int
 */

template <typename Limits>
struct AudioParameterChangeTraits<int, Limits> {
  using Parameter =
    AudioParameter<int, Limits, AudioParameterChangeTraits<int, Limits>>;

  static void apply(Parameter& p, const AudioParameterChange& change) noexcept {
    assert(change.value.is_int());
    p.target = p.clamp(change.value.data.i);
    p.value = p.target;
    p.remaining = change.frame_distance_to_target;
  }

  static int evaluate(Parameter& p) noexcept {
    p.remaining = std::max(0, p.remaining - 1);
    return p.value;
  }
};

}