#pragma once

#include "grove/common/Optional.hpp"
#include "grove/common/identifier.hpp"

namespace grove::font {

struct FontBitmapSampleInfo;

struct FontHandle {
  GROVE_INTEGER_IDENTIFIER_EQUALITY(FontHandle, id)
  GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
  uint32_t id;
};

struct ReadFontImages {
  const unsigned char* images[16];
  int num_images;
  int image_dim;
};

void initialize_fonts();
void terminate_fonts();
Optional<ReadFontImages> read_font_images();
Optional<FontHandle> get_text_font();

[[nodiscard]] int ascii_left_justified(
  FontHandle font, const char* text, float font_size, float max_width, FontBitmapSampleInfo* dst,
  float* xoff = nullptr, float* yoff = nullptr);

float get_glyph_sequence_width_ascii(FontHandle font, const char* text, float font_size);

}