#include "font.hpp"
#include "grove/gui/font.hpp"
#include "grove/common/Optional.hpp"
#include "grove/common/common.hpp"
#include "grove/common/fs.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/common/ArrayView.hpp"
#include "grove/env.hpp"
#include "grove/visual/types.hpp"
#include <vector>
#include <memory>
#include <cassert>

/*
 * @NOTE: Font handles are 1-based offsets into an array of glyph images. Thus, when adding a font
 * whose glyph set is split across multiple images, remember to increment the next font handle id
 * by the number of images.
 */

#define TEXT_FONT_HANDLE_ID (1)

GROVE_NAMESPACE_BEGIN

namespace {

Optional<std::vector<unsigned char>> read_font(const char* font_name) {
  auto font_p = std::string{GROVE_ASSET_DIR} + std::string{"/fonts/"} + font_name;
  size_t file_size{};
  if (fs::file_size(font_p, &file_size)) {
    std::vector<unsigned char> data(file_size);
    if (fs::read_bytes(font_p, data.data(), file_size, &file_size)) {
      return Optional<std::vector<unsigned char>>(std::move(data));
    }
  }
  return NullOpt{};
}

bool create_text_font(font::FontInfo** dst_font_info,
                      std::unique_ptr<unsigned char[]>* dst_image_data,
                      image::Descriptor* dst_image_desc) {
  auto arial = read_font("Arial.ttf");
  if (!arial) {
    return false;
  }

  constexpr float font_size = 24.0f;
//  float font_size_scale = 2.0f;

  unsigned char* ttf_buffer = arial.value().data();

#if 0
  constexpr int num_code_points = 4;
  int code_points[4];
  for (int i = 0; i < 4; i++) {
    code_points[i] = int('a') + i;
  }
#else
  constexpr int num_code_points = 126;
  int code_points[num_code_points];
  for (int i = 0; i < num_code_points-1; i++) {
    code_points[i] = 32 + i;
  }
  code_points[num_code_points-1] = 640;
#endif

  const int image_dim = 1024;
  font::CodePointRange code_point_range{code_points, num_code_points};
  auto store_image_data = std::make_unique<unsigned char[]>(image_dim * image_dim);
  auto* image_data = store_image_data.get();

  font::CreateFontBitmapParams create_params{};
  create_params.font_size_px = font_size;
  create_params.image_width = image_dim;
  create_params.image_height = image_dim;
  create_params.oversampling = 4;
  create_params.num_code_point_ranges = 1;
  create_params.code_point_ranges = &code_point_range;
  create_params.dst_image_data = &image_data;
  *dst_font_info = create_font_bitmaps(ttf_buffer, create_params);
  *dst_image_data = std::move(store_image_data);
  *dst_image_desc = image::Descriptor::make_2d_uint8n(image_dim, image_dim, 1);
  return true;
}

template <int N>
ArrayView<int> ascii_to_code_points(const char* text, Temporary<int, N>& store_code_points) {
  const auto num_code_points = int(std::strlen(text));
  auto* code_points = store_code_points.require(num_code_points);
  for (int i = 0; i < num_code_points; i++) {
    code_points[i] = int((unsigned char) text[i]);
  }
  return ArrayView<int>{code_points, code_points + num_code_points};
}

struct {
  font::FontInfo* text_font{};
  image::Descriptor text_image_desc{};
  std::unique_ptr<unsigned char[]> text_image_data;

  bool initialized{};
} globals;

font::FontInfo* font_info_from_handle(font::FontHandle handle) {
  assert(handle.is_valid() && globals.initialized);
  if (handle.id == TEXT_FONT_HANDLE_ID) {
    assert(globals.text_font);
    return globals.text_font;
  }
  assert(false);
  return nullptr;
}

} //  anon

void font::initialize_fonts() {
  assert(!globals.initialized);
  if (!create_text_font(&globals.text_font, &globals.text_image_data, &globals.text_image_desc)) {
    return;
  }
  globals.initialized = true;
}

void font::terminate_fonts() {
  font::destroy_font_info(&globals.text_font);
  globals.initialized = false;
}

Optional<font::ReadFontImages> font::read_font_images() {
  if (!globals.initialized) {
    return NullOpt{};
  }

  font::ReadFontImages result{};
  result.num_images = 1;
  result.images[0] = globals.text_image_data.get();
  result.image_dim = globals.text_image_desc.width();
  assert(globals.text_image_desc.width() == globals.text_image_desc.height());
  return Optional<font::ReadFontImages>(result);
}

Optional<font::FontHandle> font::get_text_font() {
  if (!globals.initialized) {
    return NullOpt{};
  } else {
    return Optional<font::FontHandle>(FontHandle{TEXT_FONT_HANDLE_ID});
  }
}

float font::get_glyph_sequence_width_ascii(FontHandle font, const char* text, float font_size) {
  assert(font.is_valid() && globals.initialized);
  Temporary<int, 2048> store_code_points;
  auto cps = ascii_to_code_points(text, store_code_points);
  auto* font_info = font_info_from_handle(font);
  return font::get_sequence_width(font_info, cps.data(), int(cps.size()), font_size);
}

int font::ascii_left_justified(FontHandle font, const char* text, float font_size,
                               float max_width, grove::font::FontBitmapSampleInfo* dst,
                               float* xoff, float* yoff) {
  assert(font.is_valid() && font.id > 0);
  assert(globals.initialized);

  Temporary<int, 2048> store_code_points;
  auto cps = ascii_to_code_points(text, store_code_points);

  auto* font_info = font_info_from_handle(font);
  const int num_gen = font::gen_bitmap_samples_left_justify(
    font_info, cps.data(), int(cps.size()), font_size, max_width, int(' '), dst, xoff, yoff);
  for (int i = 0; i < num_gen; i++) {
    //  @NOTE: Font handle is a 1-based offset into array of glyph images.
    dst[i].bitmap_index += int(font.id - 1);
  }

  return num_gen;
}

#undef TEXT_FONT_HANDLE_ID

GROVE_NAMESPACE_END
