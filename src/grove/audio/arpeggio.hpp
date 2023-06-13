#pragma once

#include "types.hpp"

namespace grove {

inline ScoreCursor next_quantum(
  const ScoreCursor& cursor, audio::Quantization quant, double tsig_num) {
  //
  double quant_beat = audio::quantize_floor(cursor.beat, quant, tsig_num);
  auto curs = ScoreCursor{cursor.measure, quant_beat};
  if (curs != cursor) {
    //  Start of next quantized period.
    const double beat_div = tsig_num / audio::quantization_divisor(quant);
    curs.wrapped_add_beats(beat_div, tsig_num);
  }
  return curs;
}

}