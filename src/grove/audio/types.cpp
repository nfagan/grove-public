#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include "types.hpp"
#include "grove/math/util.hpp"
#include <portaudio.h>
#include <cassert>

GROVE_NAMESPACE_BEGIN

namespace {

ScoreCursor positive_modulo(ScoreCursor a, ScoreCursor span, double num) {
  assert(a >= ScoreCursor{} && span > ScoreCursor{});
  if (a < span) {
    return a;
  }

  double incr{1.0};
  while (true) {
    auto ts = span;
    ts.wrapped_scale(2.0, num);
    if (ts < a) {
      span = ts;
      incr *= 2.0;
    } else {
      break;
    }
  }

  a.wrapped_sub_cursor(span, num);
  while (incr >= 1.0 && a > ScoreCursor{}) {
    if (a >= span) {
      a.wrapped_sub_cursor(span, num);
    } else {
      span.wrapped_scale(0.5, num);
      incr *= 0.5;
    }
  }

  return a;
}

ScoreCursor wrapped_around_cursor_distance(const ScoreCursor& p0, const ScoreCursor& p1,
                                           const ScoreRegion& span, double beats_per_meas) {
  const auto end = span.end(beats_per_meas);
  const auto begin = span.begin;

  auto res0 = end;
  res0.wrapped_sub_cursor(p0, beats_per_meas);

  auto res1 = p1;
  res1.wrapped_sub_cursor(begin, beats_per_meas);

  auto res = res0;
  res.wrapped_add_cursor(res1, beats_per_meas);

  return res;
}

} //  anon

unsigned long audio::to_pa_sample_format(SampleFormat format) {
  switch (format) {
    case SampleFormat::Float:
      return paFloat32;
    default:
      assert(false);
      return 0;
  }
}

/*
 * PitchClass
 */

const char* to_string(PitchClass pitch_class) {
  switch (pitch_class) {
    case PitchClass::C:
      return "C";
    case PitchClass::Cs:
      return "Cs";
    case PitchClass::D:
      return "D";
    case PitchClass::Ds:
      return "Ds";
    case PitchClass::E:
      return "E";
    case PitchClass::F:
      return "F";
    case PitchClass::Fs:
      return "Fs";
    case PitchClass::G:
      return "G";
    case PitchClass::Gs:
      return "Gs";
    case PitchClass::A:
      return "A";
    case PitchClass::As:
      return "As";
    case PitchClass::B:
      return "B";
    default:
      assert(false && "Unhandled.");
      return "";
  }
}

/*
 * SampleN
 */

static_assert(std::is_trivial<Sample2>::value,
              "Expected Sample2 to be trivial.");
static_assert(std::is_standard_layout<Sample2>::value,
              "Expected Sample2 to be a standard-layout type.");

/*
 * Quantization
 */

double audio::quantize_floor(double beat, Quantization quantization, double beats_per_measure) {
  const auto quant_divisor = beats_per_measure / quantization_divisor(quantization);
  return std::floor(beat / quant_divisor) * quant_divisor;
}

double audio::quantize_round(double beat, Quantization quantization, double beats_per_measure) {
  const auto quant_divisor = beats_per_measure / quantization_divisor(quantization);
  return std::round(beat / quant_divisor) * quant_divisor;
}

double audio::quantization_divisor(Quantization quantization) {
  switch (quantization) {
    case Quantization::SixtyFourth:
      return 64.0;
    case Quantization::ThirtySecond:
      return 32.0;
    case Quantization::Sixteenth:
      return 16.0;
    case Quantization::Eighth:
      return 8.0;
    case Quantization::Quarter:
      return 4.0;
    case Quantization::Half:
      return 2.0;
    case Quantization::Measure:
      return 1.0;
    default:
      assert(false && "Unhandled.");
      return 1.0;
  }
}

double audio::beat_divisor(Quantization quantization) {
  switch (quantization) {
    case Quantization::SixtyFourth:
      return 16.0;
    case Quantization::ThirtySecond:
      return 8.0;
    case Quantization::Sixteenth:
      return 4.0;
    case Quantization::Eighth:
      return 2.0;
    case Quantization::Quarter:
      return 1.0;
    case Quantization::Half:
      return 0.5;
    case Quantization::Measure:
      return 0.25;
    default:
      assert(false && "Unhandled.");
      return 1.0;
  }
}

/*
 * MIDINote
 */

static_assert(std::is_trivial<MIDINote>::value,
  "Expected MIDINote to be trivial.");
static_assert(std::is_standard_layout<MIDINote>::value,
  "Expected MIDINote to be a standard-layout type.");

std::size_t MIDINote::Hash::operator()(const MIDINote& note) const noexcept {
  //  @TODO: Revisit this.
  auto pc = std::hash<PitchClass>{}(note.pitch_class);
  auto oct = std::hash<int8_t>{}(note.octave);
  auto vel = std::hash<int8_t>{}(note.velocity);
  return ((pc ^ oct) ^ vel);
}

double MIDINote::frequency() const {
  return note_to_frequency(pitch_class, octave);
}

double MIDINote::semitone() const {
  return note_to_semitone(pitch_class, octave);
}

uint8_t MIDINote::note_number() const {
  return note_to_midi_note_number(pitch_class, octave);
}

bool MIDINote::matches_pitch_class_and_octave(const MIDINote& other) const {
  return pitch_class == other.pitch_class && octave == other.octave;
}

void MIDINote::transpose(int num_semitones) {
  auto st = semitone() + double(num_semitones);
  double ignore_remainder;
  semitone_to_midi_note_components(st, &pitch_class, &octave, &ignore_remainder);
}

std::string MIDINote::to_string() const {
  std::string result(grove::to_string(pitch_class));
  result += std::to_string(octave);
  result += "|";
  result += std::to_string(velocity);
  return result;
}

const MIDINote MIDINote::C3{PitchClass::C, 3, 0};
const MIDINote MIDINote::A4{PitchClass::A, 4, 0};

bool operator==(const MIDINote& a, const MIDINote& b) {
  return a.pitch_class == b.pitch_class &&
         a.octave == b.octave &&
         a.velocity == b.velocity;
}

bool operator!=(const MIDINote& a, const MIDINote& b) {
  return !(a == b);
}

bool operator<(const MIDINote& a, const MIDINote& b) {
  double st_a = a.semitone();
  double st_b = b.semitone();

  if (st_a < st_b) {
    return true;
  } else {
    return st_a == st_b && a.velocity < b.velocity;
  }
}

bool operator<=(const MIDINote& a, const MIDINote& b) {
  return !(a > b);
}

bool operator>(const MIDINote& a, const MIDINote& b) {
  double st_a = a.semitone();
  double st_b = b.semitone();

  if (st_a > st_b) {
    return true;
  } else {
    return st_a == st_b && a.velocity > b.velocity;
  }
}

bool operator>=(const MIDINote& a, const MIDINote& b) {
  return !(a < b);
}

MIDINote MIDINote::from_semitone(double st) {
  MIDINote note{};
  double ignore_remainder;
  semitone_to_midi_note_components(st, &note.pitch_class, &note.octave, &ignore_remainder);
  return note;
}

MIDINote MIDINote::from_note_number(uint8_t note_number) {
  const auto st = midi_note_number_to_semitone(note_number);
  return MIDINote::from_semitone(st);
}

/*
 * MIDIMessage
 */

static_assert(std::is_trivial<MIDIMessage>::value,
              "Expected MIDIMessage to be trivial.");
static_assert(std::is_standard_layout<MIDIMessage>::value,
              "Expected MIDIMessage to be a standard-layout type.");

uint8_t MIDIMessage::channel() const {
  return status & status_channel_mask;
}

uint8_t MIDIMessage::message() const {
  return status & status_message_mask;
}

bool MIDIMessage::is_note_on() const {
  return message() == StatusCodes::note_on;
}

bool MIDIMessage::is_note_off() const {
  return message() == StatusCodes::note_off;
}

void MIDIMessage::set_status_bit() {
  status |= status_bit_mask;
}

void MIDIMessage::set_note_number(uint8_t note_number) {
  data1 = note_number & data_mask;
}

void MIDIMessage::note_on(uint8_t channel, uint8_t note_number, uint8_t velocity) {
  status |= StatusCodes::note_on;
  status |= uint8_t(channel & status_channel_mask);
  data1 = note_number & data_mask;
  data2 = velocity & data_mask;
}

void MIDIMessage::note_off(uint8_t channel, uint8_t note_number, uint8_t velocity) {
  status |= StatusCodes::note_off;
  status |= uint8_t(channel & status_channel_mask);
  data1 = note_number & data_mask;
  data2 = velocity & data_mask;
}

double MIDIMessage::frequency() const {
  return semitone_to_frequency(semitone());
}

double MIDIMessage::semitone() const {
  return midi_note_number_to_semitone(note_number());
}

uint8_t MIDIMessage::note_number() const {
  return data1;
}

uint8_t MIDIMessage::velocity() const {
  return data2;
}

MIDIMessage MIDIMessage::make_note_on(uint8_t channel, uint8_t note_number, uint8_t velocity) {
  MIDIMessage message{};
  message.set_status_bit();
  message.note_on(channel, note_number, velocity);
  return message;
}

MIDIMessage MIDIMessage::make_note_on(uint8_t channel, const MIDINote& from_midi_note) {
  return make_note_on(channel, from_midi_note.note_number(), from_midi_note.velocity);
}

MIDIMessage MIDIMessage::make_note_off(uint8_t channel, uint8_t note_number, uint8_t velocity) {
  MIDIMessage message{};
  message.set_status_bit();
  message.note_off(channel, note_number, velocity);
  return message;
}

MIDIMessage MIDIMessage::make_note_off(uint8_t channel, const MIDINote& from_midi_note) {
  return make_note_off(channel, from_midi_note.note_number(), from_midi_note.velocity);
}

/*
 * ScoreCursor
 */

std::size_t ScoreCursor::Hash::operator()(const ScoreCursor& cursor) const noexcept {
  return std::hash<int64_t>{}(cursor.measure) ^ std::hash<double>{}(cursor.beat);
}

void ScoreCursor::zero() {
  measure = 0;
  beat = 0.0;
}

void ScoreCursor::quantize_round(audio::Quantization quantization, double beats_per_measure) {
  beat = audio::quantize_round(beat, quantization, beats_per_measure);
  wrap_beats(beats_per_measure);
}

void ScoreCursor::quantize_floor(audio::Quantization quantization, double beats_per_measure) {
  beat = audio::quantize_floor(beat, quantization, beats_per_measure);
  wrap_beats(beats_per_measure);
}

std::string ScoreCursor::to_string() const {
  auto result = std::to_string(measure);
  result += ".";
  result += std::to_string(beat);
  return result;
}

ScoreCursor ScoreCursor::from_beats(double beats, double beats_per_measure) {
  ScoreCursor result{};
  result.wrapped_add_beats(beats, beats_per_measure);
  return result;
}

ScoreCursor modulo(ScoreCursor a, ScoreCursor span, double num) {
  assert(span > ScoreCursor{});
  if (a < ScoreCursor{}) {
    auto dist = span;
    dist.wrapped_sub_cursor(a, num);
    auto res = positive_modulo(dist, span, num);
    auto tmp = span;
    tmp.wrapped_sub_cursor(res, num);
    return positive_modulo(tmp, span, num);
  } else {
    return positive_modulo(a, span, num);
  }
}

/*
 * ScoreRegion
 */

std::size_t ScoreRegion::Hash::operator()(const ScoreRegion& cursor) const noexcept {
  auto hash = ScoreCursor::Hash{};
  return hash(cursor.begin) ^ hash(cursor.size);
}

int partition_loop(const ScoreRegion& source, const ScoreRegion& loop, double num,
                   ScoreRegionSegment* dst, int max_num_dst) {
  assert(loop.contains(source.begin, num));
  ScoreCursor off{};

  auto beg = source.begin;
  auto source_size = source.size;
  const auto loop_end = make_score_cursor_view(loop.end(num), num);

  int dst_ind{};
  ScoreCursor rem = source_size;
  while (rem > ScoreCursor{}) {
    auto seg_size = std::min((loop_end - beg).cursor, rem);
    if (dst_ind < max_num_dst) {
      dst[dst_ind] = ScoreRegionSegment{ScoreRegion{beg, seg_size}, off};
    }
    dst_ind++;
    off.wrapped_add_cursor(seg_size, num);
    rem.wrapped_sub_cursor(seg_size, num);
    beg = loop.begin;
  }

  return dst_ind;
}

ScoreCursor le_order_dependent_cursor_distance(const ScoreCursor& p0,
                                               const ScoreCursor& p1,
                                               const ScoreRegion& span,
                                               double beats_per_meas) {
  if (p0 <= p1) {
    auto res = p1;
    res.wrapped_sub_cursor(p0, beats_per_meas);
    return res;
  } else {
    return wrapped_around_cursor_distance(p0, p1, span, beats_per_meas);
  }
}

ScoreCursor lt_order_dependent_cursor_distance(const ScoreCursor& p0,
                                               const ScoreCursor& p1,
                                               const ScoreRegion& span,
                                               double beats_per_meas) {
  if (p0 < p1) {
    auto res = p1;
    res.wrapped_sub_cursor(p0, beats_per_meas);
    return res;
  } else {
    return wrapped_around_cursor_distance(p0, p1, span, beats_per_meas);
  }
}

/*
 * SegmentedScoreRegion
 */

ScoreRegion& SegmentedScoreRegion::operator[](int index) {
  assert(index >= 0 && index < num_segments);
  return segments[index];
}

const ScoreRegion& SegmentedScoreRegion::operator[](int index) const {
  assert(index >= 0 && index < num_segments);
  return segments[index];
}

SegmentedScoreRegion SegmentedScoreRegion::make_note_segments(ScoreRegion note_span,
                                                              ScoreRegion clip_span,
                                                              double beats_per_measure) {
  assert(note_span.size <= clip_span.size);
  SegmentedScoreRegion segments{};

  auto note_begin = clip_span.keep_within(note_span.begin, beats_per_measure);
  auto note_end = note_begin;
  note_end.wrapped_add_cursor(note_span.size, beats_per_measure);
  note_end = clip_span.keep_within(note_end, beats_per_measure);

  segments.segments[0].begin = note_begin;

  if (note_end > note_begin) {
    //  Note end is within [span.begin, span.begin + span.size)
    segments.segments[0].size = note_span.size;
    segments.num_segments = 1;

  } else {
    //  Two segments: [note.begin, span.size) ; [span.begin, note_end)
    auto head_end = clip_span.end(beats_per_measure);
    head_end.wrapped_sub_cursor(note_begin, beats_per_measure);
    segments.segments[0].size = head_end;

    note_end.wrapped_sub_cursor(clip_span.begin, beats_per_measure);
    segments.segments[1].begin = clip_span.begin;
    segments.segments[1].size = note_end;
    segments.num_segments = 2;
  }

  return segments;
}

/*
 * ScheduableMIDINoteIDStore
 */

ScheduableMIDINoteID ScheduableMIDINoteIDStore::create() {
  return ScheduableMIDINoteIDStore::next_id++;
}

//  Reserve 0 for null id.
std::atomic<ScheduableMIDINoteID> ScheduableMIDINoteIDStore::next_id{1};

/*
 * ScheduableMIDINote
 */

ScheduableMIDINote::ScheduableMIDINote() :
  ScheduableMIDINote(MIDINote::C3, ScoreCursor{}, 0.0, null_scheduable_midi_note_id()) {
  //
}

ScheduableMIDINote::ScheduableMIDINote(const MIDINote& note,
                                       ScoreCursor start,
                                       double beat_duration,
                                       ScheduableMIDINoteID id) :
  note(note),
  start(start),
  beat_duration(beat_duration),
  id(id) {
  //
}

/*
* NoteUtil
*/

double note_to_semitone(PitchClass note, int octave) {
  const auto n = double(note);
  const auto a = double(reference_pitch_class());
  return (n - a) + (double(octave) - double(reference_octave())) * 12.0;
}

double note_to_frequency(PitchClass note, int octave) {
  const double st = note_to_semitone(note, octave);
  return semitone_to_frequency(st);
}

double semitone_to_frequency(double semitone) {
  double st = semitone - semitone_a4();
  return frequency_a4() * std::pow(2.0, st/12.0);
}

double semitone_to_rate_multiplier(double semitone) {
  return std::pow(2.0, semitone/12.0);
}

double midi_note_number_to_semitone(uint8_t note_number) {
  auto off = double(note_number) - double(midi_note_number_c3());
  return off + semitone_c3();
}

uint8_t semitone_to_midi_note_number(double st) {
  auto num = (st - semitone_c3()) + double(midi_note_number_c3());
  num = grove::clamp(num, 0.0, 255.0);
  return uint8_t(num);
}

uint8_t note_to_midi_note_number(PitchClass note, int octave) {
  return semitone_to_midi_note_number(note_to_semitone(note, octave));
}

void semitone_to_midi_note_components(double semitone,
                                      PitchClass* pitch_class,
                                      int8_t* octave,
                                      double* remainder) {
#ifdef GROVE_DEBUG
  if (!std::isfinite(semitone)) {
    GROVE_LOG_WARNING_CAPTURE_META("Non-finite semitone.", "semitone_to_midi_note_components");
    *pitch_class = PitchClass::C;
    *octave = 0;
    *remainder = 0;
    return;
  }
#endif
  double abs_st = std::abs(semitone);
  double st_base = std::floor(abs_st);
  *remainder = abs_st - st_base;

  auto st = int(st_base) * (semitone < 0 ? -1 : 1);
  int8_t oct = reference_octave();
  count_wraps_to_range(&st, 12, &oct);

  auto a_relative = st + int(reference_pitch_class());
  if (a_relative >= 12) {
    oct++;
    a_relative %= 12;
  }

  *pitch_class = PitchClass(a_relative);
  *octave = oct;
}

double TimeSignature::beats_per_sample_at_bpm(double bpm, double sample_rate) const {
  return grove::beats_per_sample_at_bpm(bpm, sample_rate, *this);
}

double TimeSignature::beats_per_measure() const {
  return double(numerator);
}

GROVE_NAMESPACE_END