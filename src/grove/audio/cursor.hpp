#pragma once

#include "types.hpp"

namespace grove {

struct QuantizedScoreCursor {
  enum class Depth {
    D4 = 0,
    D16 = 1,
    D64 = 2,
    D256 = 3,
    D1024 = 4,
    D4096 = 5,
    D16384 = 6,
    D65536 = 7,
    D262144 = 8,
    Max = 8
  };

  int32_t measure;
  int16_t beat;
  uint16_t division;
};

inline ScoreCursor decode(QuantizedScoreCursor cursor) {
  double rem{};
  const uint32_t four = uint32_t(1) << 2u;
  for (int i = 0; i < 8; i++) {
    uint16_t m16 = uint16_t(3) << (i * 2);
    uint16_t val = cursor.division & m16;
    val >>= uint16_t(i * 2);
    rem += double(val) / double(four << (2 * i));
  }

  ScoreCursor result{};
  result.measure = cursor.measure;
  result.beat = double(cursor.beat) + rem;
  return result;
}

inline QuantizedScoreCursor encode(const ScoreCursor& cursor, QuantizedScoreCursor::Depth depth,
                                   double* leftover) {
  assert(cursor.beat >= 0.0 && "Wrap beats before encoding.");

  double dst = std::floor(cursor.beat);
  double rem = cursor.beat - dst;

  uint16_t encoded{};
  const uint32_t four = uint32_t(1) << 2u;
  for (int i = 0; i < int(depth); i++) {
    const auto f = double(four << (2 * i));
    const double db = std::floor(rem * f);
    assert(db >= 0.0 && db < 4.0 && (db - std::floor(db) == 0.0));
    rem -= db / f;
    auto v = uint16_t(db);
    v <<= i * 2;
    encoded |= v;
  }

  QuantizedScoreCursor result{};
  result.measure = int32_t(cursor.measure);
  result.beat = int16_t(dst);
  result.division = encoded;
  *leftover = rem;
  return result;
}

inline QuantizedScoreCursor encode(const ScoreCursor& cursor, QuantizedScoreCursor::Depth depth) {
  double ignore{};
  return encode(cursor, depth, &ignore);
}

}