#pragma once

#include "grove/common/DynamicArray.hpp"
#include "grove/common/util.hpp"
#include <functional>
#include <string>
#include <limits>
#include <cstdint>
#include <cmath>
#include <atomic>

#define GROVE_NULL_MEASURE_ID (int64_t(-1))

struct PaStreamCallbackTimeInfo;

namespace grove {

namespace audio {
  enum class SampleFormat {
    Float = 0
  };

  unsigned long to_pa_sample_format(SampleFormat format);

  using AudioProcessCallback =
    int (*)(const void*, void*, unsigned long,
            const PaStreamCallbackTimeInfo*, unsigned long, void*);
}

/*
 * Sample
 */

using Sample = float;

template <int N>
struct Samples {
  void assign(Sample scalar);

  Samples& operator-=(const Samples& other);
  Samples& operator+=(const Samples& other);
  Samples& operator*=(const Samples& other);
  Samples& operator/=(const Samples& other);

  Sample samples[N];
};

template <int N>
void Samples<N>::assign(Sample scalar) {
  for (int i = 0; i < N; i++) {
    samples[i] = scalar;
  }
}

template <int N>
Samples<N>& Samples<N>::operator+=(const Samples<N>& other) {
  for (int i = 0; i < N; i++) {
    samples[i] += other.samples[i];
  }
  return *this;
}

template <int N>
Samples<N>& Samples<N>::operator-=(const Samples<N>& other) {
  for (int i = 0; i < N; i++) {
    samples[i] -= other.samples[i];
  }
  return *this;
}

template <int N>
Samples<N>& Samples<N>::operator*=(const Samples<N>& other) {
  for (int i = 0; i < N; i++) {
    samples[i] *= other.samples[i];
  }
  return *this;
}

template <int N>
Samples<N>& Samples<N>::operator/=(const Samples<N>& other) {
  for (int i = 0; i < N; i++) {
    samples[i] /= other.samples[i];
  }
  return *this;
}

template <int N>
inline Samples<N> operator+(const Samples<N>& a, const Samples<N>& b) {
  Samples<N> res;
  for (int i = 0; i < N; i++) {
    res.samples[i] = a.samples[i] + b.samples[i];
  }
  return res;
}

template <int N>
inline Samples<N> operator-(const Samples<N>& a, const Samples<N>& b) {
  Samples<N> res;
  for (int i = 0; i < N; i++) {
    res.samples[i] = a.samples[i] - b.samples[i];
  }
  return res;
}

template <int N>
inline Samples<N> operator*(const Samples<N>& a, const Samples<N>& b) {
  Samples<N> res;
  for (int i = 0; i < N; i++) {
    res.samples[i] = a.samples[i] * b.samples[i];
  }
  return res;
}

template <int N>
inline Samples<N> operator/(const Samples<N>& a, const Samples<N>& b) {
  Samples<N> res;
  for (int i = 0; i < N; i++) {
    res.samples[i] = a.samples[i] / b.samples[i];
  }
  return res;
}

template <int N>
inline Samples<N> operator+(const Samples<N>& a, Sample b) {
  Samples<N> res;
  for (int i = 0; i < N; i++) {
    res.samples[i] = a.samples[i] + b;
  }
  return res;
}

template <int N>
inline Samples<N> operator+(Sample a, const Samples<N>& b) {
  Samples<N> res;
  for (int i = 0; i < N; i++) {
    res.samples[i] = a + b.samples[i];
  }
  return res;
}

template <int N>
inline Samples<N> operator*(const Samples<N>& a, Sample b) {
  Samples<N> res;
  for (int i = 0; i < N; i++) {
    res.samples[i] = a.samples[i] * b;
  }
  return res;
}

template <int N>
inline Samples<N> operator*(Sample a, const Samples<N>& b) {
  Samples<N> res;
  for (int i = 0; i < N; i++) {
    res.samples[i] = a * b.samples[i];
  }
  return res;
}

using Sample2 = Samples<2>;

/*
 * PitchClass
 */

enum class PitchClass : int8_t {
  C = 0,
  Cs,
  D,
  Ds,
  E,
  F,
  Fs,
  G,
  Gs,
  A,
  As,
  B
};

constexpr int num_pitch_classes() {
  return 12;
}

constexpr PitchClass reference_pitch_class() {
  return PitchClass::C;
}

constexpr int8_t reference_octave() {
  return 3;
}

constexpr double reference_semitone() {
  constexpr auto a = double(reference_pitch_class());
  return a + double(reference_octave()) * 12.0;
}

//  @TODO: At some point used the wrong note number here as a reference. Slowly going through
//  and fixing things, but creates some subtle issues.
constexpr uint8_t midi_note_number_c3() {
  return 60;
}

constexpr uint8_t midi_note_number_a4() {
  return 69;
}

constexpr double frequency_a4() {
  return 440.0;
}

constexpr double default_sample_rate() {
  return 44.1e3;
}

constexpr double semitone_c3() {
  constexpr auto n = double(PitchClass::C);
  constexpr auto a = double(reference_pitch_class());
  return (n - a) + (3.0 - double(reference_octave())) * 12.0;
}

constexpr double semitone_a4() {
  constexpr auto n = double(PitchClass::A);
  constexpr auto a = double(reference_pitch_class());
  return (n - a) + (4.0 - double(reference_octave())) * 12.0;
}

const char* to_string(PitchClass pitch_class);

/*
 * Quantization
 */

namespace audio {
  enum class Quantization : int8_t {
    SixtyFourth = 0,
    ThirtySecond,
    Sixteenth,
    Eighth,
    Quarter,
    Half,
    Measure,
    QUANTIZATION_SIZE
  };

  double quantization_divisor(Quantization quantization);
  double beat_divisor(Quantization quantization);

  double quantize_floor(double beat, Quantization quantization, double beats_per_measure);
  double quantize_round(double beat, Quantization quantization, double beats_per_measure);
}

/*
 * MIDINote
 */

struct MIDINote {
public:
  struct Hash {
    //  @TODO: Revisit this.
    std::size_t operator()(const MIDINote& note) const noexcept;
  };

public:
  double frequency() const;
  double semitone() const;
  uint8_t note_number() const;

  std::string to_string() const;
  bool matches_pitch_class_and_octave(const MIDINote& other) const;

  void transpose(int num_semitones);

  friend bool operator==(const MIDINote& a, const MIDINote& b);
  friend bool operator!=(const MIDINote& a, const MIDINote& b);

  friend bool operator<(const MIDINote& a, const MIDINote& b);
  friend bool operator<=(const MIDINote& a, const MIDINote& b);

  friend bool operator>(const MIDINote& a, const MIDINote& b);
  friend bool operator>=(const MIDINote& a, const MIDINote& b);

  static MIDINote from_semitone(double st);
  static MIDINote from_note_number(uint8_t note_number);

public:
  PitchClass pitch_class;
  int8_t octave;
  int8_t velocity;

  static const MIDINote C3;
  static const MIDINote A4;
};

using MIDINotes = DynamicArray<MIDINote, 16>;

/*
 * MIDIMessage
 */

struct MIDIMessage {
public:
  struct StatusCodes {
    static constexpr uint8_t note_on = 0x80;
    static constexpr uint8_t note_off = 0x90;
  };

  static constexpr uint8_t status_channel_mask = 0x0f;
  static constexpr uint8_t status_message_mask = 0xf0;
  static constexpr uint8_t status_bit_mask = uint8_t(1u) << uint8_t(7u);
  static constexpr uint8_t data_mask = 0x7f;

public:
  uint8_t channel() const;
  uint8_t message() const;
  bool is_note_on() const;
  bool is_note_off() const;
  void set_status_bit();
  void set_note_number(uint8_t note_number);

  void note_on(uint8_t channel, uint8_t note_number, uint8_t velocity);
  void note_off(uint8_t channel, uint8_t note_number, uint8_t velocity);

  double frequency() const;
  double semitone() const;
  uint8_t note_number() const;
  uint8_t velocity() const;

  static MIDIMessage make_note_on(uint8_t channel, uint8_t note_number, uint8_t velocity);
  static MIDIMessage make_note_on(uint8_t channel, const MIDINote& from_midi_note);

  static MIDIMessage make_note_off(uint8_t channel, uint8_t note_number, uint8_t velocity);
  static MIDIMessage make_note_off(uint8_t channel, const MIDINote& from_midi_note);

public:
  uint8_t status;
  uint8_t data1;
  uint8_t data2;
};

using MIDIMessages = DynamicArray<MIDIMessage, 16>;

/*
 * ScoreCursor
 */

struct ScoreCursor {
public:
  struct Hash {
    std::size_t operator()(const ScoreCursor& cursor) const noexcept;
  };

public:
  friend bool operator==(const ScoreCursor& a, const ScoreCursor& b);
  friend bool operator!=(const ScoreCursor& a, const ScoreCursor& b);
  friend bool operator<(const ScoreCursor& a, const ScoreCursor& b);
  friend bool operator<=(const ScoreCursor& a, const ScoreCursor& b);
  friend bool operator>(const ScoreCursor& a, const ScoreCursor& b);
  friend bool operator>=(const ScoreCursor& a, const ScoreCursor& b);

  //  Increment the cursor by `beat_increment` beats.
  void wrapped_add_beats(double beat_increment, double beats_per_measure);
  //  Increment the cursor by `other`.
  void wrapped_add_cursor(const ScoreCursor& other, double beats_per_measure);
  //  Decrement the cursor by `other`.
  void wrapped_sub_cursor(const ScoreCursor& other, double beats_per_measure);
  //  Scale the cursor by `scalar`.
  void wrapped_scale(double scalar, double beats_per_measure);
  //  Wrap beats within `beats_per_measure` and apply the increment, if there is one.
  void wrap_beats(double beats_per_measure);

  void quantize_round(audio::Quantization quantization, double beats_per_measure);
  void quantize_floor(audio::Quantization quantization, double beats_per_measure);

  //  Set the cursor to 0.
  void zero();

  double to_beats(double beats_per_measure) const;
  double to_sample_offset(double samples_per_beat, double beats_per_measure) const;
  std::string to_string() const;

  static ScoreCursor from_beats(double beats, double beats_per_measure);

public:
  int64_t measure;
  double beat;
};

/*
 * ScoreCursor impl.
 */

inline void ScoreCursor::wrapped_scale(double scalar, double beats_per_measure) {
#if 1
  beat *= scalar;
  auto new_meas = double(measure) * scalar;
  auto floor_meas = std::floor(new_meas);
  beat += (new_meas - floor_meas) * beats_per_measure;
  measure = int64_t(floor_meas);
  count_wraps_to_range(&beat, beats_per_measure, &measure);
#else
  //  Greater risk of overflow by converting to beats and performing the scale there.
  auto beats = to_beats(beats_per_measure);
  beats *= scalar;
  *this = ScoreCursor::from_beats(beats, beats_per_measure);
#endif
}

//  Express ScoreCursor `a` within the range implicitly given by [0, b).
//  `b` must have non-negative components, and either `b.measure` or `b.beat` must be strictly
//  greater than 0. Additionally, `b.beat` must be strictly less than `beats_per_measure`.
//  `a` can have any combination of negative or positive components, and `a.beat` can be greater
//  than `beats_per_measure`.
inline ScoreCursor wrap_within_range(const ScoreCursor& a,
                                     const ScoreCursor& b,
                                     double beats_per_measure) {
  assert(b.measure >= 0 && (b.measure > 0 || b.beat > 0.0) && b.beat < beats_per_measure);
  int64_t measure = a.measure;
  double beat = a.beat;
  count_wraps_to_range(&beat, beats_per_measure, &measure);
  //  beat is now in range [0.0, beats_per_measure).
  if (measure < 0 || measure > b.measure) {
    measure = b.measure == 0 ? 0 : wrap_within_range(measure, b.measure);
  }
  //  measure is now in inclusive range [0, b.measure].
  if (measure < b.measure || (measure == b.measure && beat < b.beat)) {
    //  Strictly less than b.
    return ScoreCursor{measure, beat};
  } else {
    //  measure == b.measure, beat >= b.beat.
    if (b.beat != 0.0) {
      beat = wrap_within_range(beat, b.beat);
    }
//    measure = b.measure == 0 ? 0 : wrap_within_range(measure, b.measure);
    measure = 0;
    return ScoreCursor{measure, beat};
  }
}

inline void ScoreCursor::wrapped_add_cursor(const ScoreCursor& other,
                                            double beats_per_measure) {
  int64_t beat_incr = 0;
  beat += other.beat;
  count_wraps_to_range(&beat, beats_per_measure, &beat_incr);
  measure += other.measure + beat_incr;
}

inline void ScoreCursor::wrapped_sub_cursor(const ScoreCursor& other,
                                            double beats_per_measure) {
  int64_t beat_incr = 0;
  beat -= other.beat;
  measure -= other.measure;
  count_wraps_to_range(&beat, beats_per_measure, &beat_incr);
  measure += beat_incr;
}

inline void ScoreCursor::wrapped_add_beats(double beat_increment, double beats_per_measure) {
  beat += beat_increment;
  count_wraps_to_range(&beat, beats_per_measure, &measure);
}

inline void ScoreCursor::wrap_beats(double beats_per_measure) {
  count_wraps_to_range(&beat, beats_per_measure, &measure);
}

inline double ScoreCursor::to_beats(double beats_per_measure) const {
  return double(measure) * beats_per_measure + beat;
}

inline double ScoreCursor::to_sample_offset(double samples_per_beat,
                                            double beats_per_measure) const {
  return samples_per_beat * to_beats(beats_per_measure);
}

inline bool operator==(const ScoreCursor& a, const ScoreCursor& b) {
  return a.measure == b.measure && a.beat == b.beat;
}

inline bool operator!=(const ScoreCursor& a, const ScoreCursor& b) {
  return !(a == b);
}

inline bool operator<(const ScoreCursor& a, const ScoreCursor& b) {
  return a.measure < b.measure || (a.measure == b.measure && a.beat < b.beat);
}

inline bool operator<=(const ScoreCursor& a, const ScoreCursor& b) {
  return !(a > b);
}

inline bool operator>(const ScoreCursor& a, const ScoreCursor& b) {
  return a.measure > b.measure || (a.measure == b.measure && a.beat > b.beat);
}

inline bool operator>=(const ScoreCursor& a, const ScoreCursor& b) {
  return !(a < b);
}

//  Compute a % span using a potentially expensive iterative procedure. `a` can have negative
//  components, but `span` must be strictly positive.
ScoreCursor modulo(ScoreCursor a, ScoreCursor span, double num);

/*
 * ScoreRegion
 */

struct ScoreRegion {
public:
  struct Hash {
    std::size_t operator()(const ScoreRegion& cursor) const noexcept;
  };

public:
  static ScoreRegion from_begin_end(ScoreCursor begin, ScoreCursor end, double beats_per_measure) {
    ScoreRegion result;
    result.begin = begin;
    result.size = end;
    result.size.wrapped_sub_cursor(begin, beats_per_measure);
    return result;
  }

  //  Ensure `cursor` lies within the region by wrapping its components. Produces the same result
  //  as (cursor - begin) % size + begin only when `size` contains integer components.
  ScoreCursor keep_within(ScoreCursor cursor, double beats_per_measure) const;

  //  Compute (cursor % size) + begin using a potentially expensive iterative procedure.
  ScoreCursor loop(ScoreCursor cursor, double beats_per_measure) const;

  //  Calculate C = begin + size.
  ScoreCursor end(double beats_per_measure) const;

  //  True if the region has size 0.
  bool empty() const {
    return size == ScoreCursor{};
  }

  //  True if two ScoreRegions overlap.
  bool intersects(const ScoreRegion& other, double beats_per_measure) const;

  //  True if `cursor` is within [begin, begin + size)
  bool contains(const ScoreCursor& cursor, double beats_per_measure) const;

  friend bool operator==(const ScoreRegion& a, const ScoreRegion& b);
  friend bool operator!=(const ScoreRegion& a, const ScoreRegion& b);
  friend bool operator<(const ScoreRegion& a, const ScoreRegion& b);

public:
  ScoreCursor begin;
  ScoreCursor size;
};

/*
 * ScoreRegion impl.
 */

inline bool operator==(const ScoreRegion& a, const ScoreRegion& b) {
  return a.begin == b.begin && a.size == b.size;
}

inline bool operator!=(const ScoreRegion& a, const ScoreRegion& b) {
  return !(a == b);
}

inline bool operator<(const ScoreRegion& a, const ScoreRegion& b) {
  return a.begin < b.begin || (a.begin == b.begin && a.size < b.size);
}

inline ScoreCursor ScoreRegion::keep_within(ScoreCursor cursor, double beats_per_measure) const {
  cursor.wrapped_sub_cursor(begin, beats_per_measure);
  cursor = wrap_within_range(cursor, size, beats_per_measure);
  cursor.wrapped_add_cursor(begin, beats_per_measure);
  return cursor;
}

inline ScoreCursor ScoreRegion::loop(ScoreCursor cursor, double beats_per_measure) const {
//  cursor.wrapped_sub_cursor(begin, beats_per_measure);
  cursor = modulo(cursor, size, beats_per_measure);
  cursor.wrapped_add_cursor(begin, beats_per_measure);
  return cursor;
}

inline ScoreCursor ScoreRegion::end(double beats_per_measure) const {
  auto beg = begin;
  beg.wrapped_add_cursor(size, beats_per_measure);
  return beg;
}

inline bool ScoreRegion::contains(const ScoreCursor& cursor, double beats_per_measure) const {
  return cursor >= begin && cursor < end(beats_per_measure);
}

inline bool ScoreRegion::intersects(const ScoreRegion& other, double beats_per_measure) const {
  auto self_end = end(beats_per_measure);
  auto other_end = other.end(beats_per_measure);
  if (begin <= other.begin) {
    return other.begin < self_end;
  } else {
    return begin < other_end;
  }
}

inline ScoreRegion union_of(const ScoreRegion& a, const ScoreRegion& b, double beats_per_measure) {
  auto a_beg = a.begin;
  auto b_beg = b.begin;
  auto a_end = a.end(beats_per_measure);
  auto b_end = b.end(beats_per_measure);
  auto beg = std::min(a_beg, b_beg);
  auto end = std::max(a_end, b_end);
  auto sz = end;
  sz.wrapped_sub_cursor(beg, beats_per_measure);
  return ScoreRegion{beg, sz};
}

inline ScoreRegion intersect_of(const ScoreRegion& a, const ScoreRegion& b, double beats_per_measure) {
  auto a_beg = a.begin;
  auto b_beg = b.begin;
  auto a_end = a.end(beats_per_measure);
  auto b_end = b.end(beats_per_measure);
  auto beg = std::max(a_beg, b_beg);
  auto end = std::min(a_end, b_end);
  auto sz = end;
  sz.wrapped_sub_cursor(beg, beats_per_measure);
  return ScoreRegion{beg, sz};
}

struct ScoreRegionSegment {
  ScoreRegion span;
  ScoreCursor cumulative_offset;
};

//  Split the interval `source` beginning within `loop` into non-overlapping segments residing
//  strictly within `loop`.
int partition_loop(const ScoreRegion& source, const ScoreRegion& loop, double num,
                   ScoreRegionSegment* dst, int max_num_dst);

//  Obtain the distance between p0 and p1 in which p0 is meant to precede p1 in absolute time,
//  but where p0 may occur after p1 in score-relative time. If p1 occurs strictly after
//  p0 in score-relative time, then the result is simply (conceptually) p1 - p0. However, if
//  p1 occurs at or before p0 in score-relative time, then the result is
//  (span.end - p0) + (p1 - span.begin). For the same point (p0 == p1), the distance
//  between them is the size of the span.
ScoreCursor lt_order_dependent_cursor_distance(const ScoreCursor& p0,
                                               const ScoreCursor& p1,
                                               const ScoreRegion& span,
                                               double beats_per_meas);

//  Obtain the distance between p0 and p1 in which p0 is meant to precede p1 in absolute time,
//  but where p0 may occur after p1 in score-relative time. If p1 occurs at the same time or
//  after p0 in score-relative time, then the result is simply (conceptually) p1 - p0. However,
//  if p1 occurs before p0 in score-relative time, then the result is (span.end - p0) +
//  (p1 - span.begin). For the same point (p0 == p1), the distance between them is 0.
ScoreCursor le_order_dependent_cursor_distance(const ScoreCursor& p0,
                                               const ScoreCursor& p1,
                                               const ScoreRegion& span,
                                               double beats_per_meas);

//  Represents a region of a score (usually, a note) that might be split into two segments
//  because it wraps around to the beginning of a looped clip.
struct SegmentedScoreRegion {
public:
  ScoreRegion& operator[](int index);
  const ScoreRegion& operator[](int index) const;

  static SegmentedScoreRegion make_note_segments(ScoreRegion note_span,
                                                 ScoreRegion clip_span,
                                                 double beats_per_measure);
public:
  ScoreRegion segments[2];
  int num_segments;
};

/*
 * ScoreCursorView
 */

struct ScoreCursorView {
  ScoreCursorView& operator+=(ScoreCursor other) {
    cursor.wrapped_add_cursor(other, beats_per_measure);
    return *this;
  }
  ScoreCursorView& operator+=(ScoreCursorView other) {
    assert(other.beats_per_measure == beats_per_measure);
    cursor.wrapped_add_cursor(other.cursor, beats_per_measure);
    return *this;
  }
  ScoreCursorView& operator-=(ScoreCursor other) {
    cursor.wrapped_sub_cursor(other, beats_per_measure);
    return *this;
  }
  ScoreCursorView& operator-=(ScoreCursorView other) {
    assert(other.beats_per_measure == beats_per_measure);
    cursor.wrapped_sub_cursor(other.cursor, beats_per_measure);
    return *this;
  }

  ScoreCursor cursor;
  double beats_per_measure;
};

inline ScoreCursorView make_score_cursor_view(ScoreCursor a, double beats_per_measure) {
  a.wrap_beats(beats_per_measure);
  return ScoreCursorView{a, beats_per_measure};
}

inline ScoreCursorView operator+(ScoreCursorView a, ScoreCursor other) {
  a.cursor.wrapped_add_cursor(other, a.beats_per_measure);
  return a;
}

inline ScoreCursorView operator+(ScoreCursorView a, ScoreCursorView other) {
  assert(a.beats_per_measure == other.beats_per_measure);
  a.cursor.wrapped_add_cursor(other.cursor, a.beats_per_measure);
  return a;
}

inline ScoreCursorView operator-(ScoreCursorView a, ScoreCursor other) {
  a.cursor.wrapped_sub_cursor(other, a.beats_per_measure);
  return a;
}

inline ScoreCursorView operator-(ScoreCursorView a, ScoreCursorView other) {
  assert(a.beats_per_measure == other.beats_per_measure);
  a.cursor.wrapped_sub_cursor(other.cursor, a.beats_per_measure);
  return a;
}

/*
 * ScheduledMIDINoteID
 */

using ScheduableMIDINoteID = uint64_t;
constexpr uint64_t null_scheduable_midi_note_id() {
  return 0;
}

class ScheduableMIDINoteIDStore {
public:
  static ScheduableMIDINoteID create();

private:
  static std::atomic<ScheduableMIDINoteID> next_id;
};

/*
 * ScheduledMIDINote
 */

struct ScheduableMIDINote {
public:
  struct Hash {
    std::size_t operator()(const ScheduableMIDINote& nt) const noexcept;
  };

public:
  ScheduableMIDINote();
  ScheduableMIDINote(const MIDINote& note, ScoreCursor start,
                     double beat_duration, ScheduableMIDINoteID id);

  ScoreRegion to_span(double beats_per_measure) const;
  ScoreCursor end(double beats_per_measure) const;

  friend bool operator==(const ScheduableMIDINote& a, const ScheduableMIDINote& b);
  friend bool operator!=(const ScheduableMIDINote& a, const ScheduableMIDINote& b);

public:
  MIDINote note;
  ScoreCursor start;
  double beat_duration;
  ScheduableMIDINoteID id;
};

/*
* impl
*/

inline ScoreCursor ScheduableMIDINote::end(double beats_per_measure) const {
  auto res = start;
  res.wrapped_add_beats(beat_duration, beats_per_measure);
  return res;
}

inline ScoreRegion ScheduableMIDINote::to_span(double beats_per_measure) const {
  return ScoreRegion{start, ScoreCursor::from_beats(beat_duration, beats_per_measure)};
}

inline std::size_t ScheduableMIDINote::Hash::operator()(const ScheduableMIDINote& nt) const noexcept {
  return std::hash<ScheduableMIDINoteID>{}(nt.id);
}

inline bool operator==(const ScheduableMIDINote& a, const ScheduableMIDINote& b) {
  return a.id == b.id &&
         a.note == b.note && a.start == b.start && a.beat_duration == b.beat_duration;
}

inline bool operator!=(const ScheduableMIDINote& a, const ScheduableMIDINote& b) {
  return !(a == b);
}

struct ClipNote {
  friend inline bool operator==(const ClipNote& a, const ClipNote& b) {
    return a.span == b.span && a.note == b.note;
  }
  friend inline bool operator!=(const ClipNote& a, const ClipNote& b) {
    return !(a == b);
  }
  bool intersects(const ClipNote& b, double beats_per_measure) const {
    return note.octave == b.note.octave &&
           note.pitch_class == b.note.pitch_class &&
           span.intersects(b.span, beats_per_measure);
  }

  ScoreRegion span;
  MIDINote note;
};

/*
 * NoteUtil
 */

double note_to_semitone(PitchClass note, int octave);
double note_to_frequency(PitchClass note, int octave);
double semitone_to_frequency(double semitone);
double semitone_to_rate_multiplier(double semitone);

void semitone_to_midi_note_components(double semitone,
                                      PitchClass* pitch_class,
                                      int8_t* octave,
                                      double* remainder);

uint8_t semitone_to_midi_note_number(double st);
uint8_t note_to_midi_note_number(PitchClass note, int octave);

double midi_note_number_to_semitone(uint8_t note_number);

/*
* AudioRenderInfo
*/

struct AudioRenderInfo {
  double sample_rate;
  int num_frames;
  int num_channels;
  uint64_t render_frame;
};

/*
 * GainUtil
 */

constexpr double minimum_finite_gain() {
  return -70.0;
}

constexpr double minimum_gain() {
  return -std::numeric_limits<double>::infinity();
}

inline double amplitude_to_db(double amp) {
  return 20.0 * std::log10(amp);
}

inline double db_to_amplitude(double db) {
  return std::pow(10.0, db / 20.0);
}

inline bool is_minimum_gain(double value) {
  return value == minimum_gain();
}

/*
 * TransportUtil
 */

struct TimeSignature {
  constexpr TimeSignature() : TimeSignature(4, 4) {}
  constexpr TimeSignature(int8_t num, int8_t denom) : numerator{num}, denominator{denom} {}

  double beats_per_sample_at_bpm(double bpm, double sample_rate) const;
  double beats_per_measure() const;

  int8_t numerator;
  int8_t denominator;
};

inline double bpm_to_bps(double bpm) {
  return bpm / 60.0;
}

inline double beats_per_sample_at_bpm(double bpm, double sample_rate,
                                      const TimeSignature& time_signature) {
  return (bpm_to_bps(bpm) / sample_rate) * (double(time_signature.denominator) / 4.0);
}

inline double frame_index_increment(double src_sr, double output_sr, double rate_multiplier) {
  return rate_multiplier * (src_sr / output_sr);
}

constexpr TimeSignature reference_time_signature() {
  return TimeSignature{4, 4};
}

}