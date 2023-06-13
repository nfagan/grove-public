#include "font.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Optional.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include <vector>
#include <unordered_map>

GROVE_NAMESPACE_BEGIN

namespace font {

struct FontInfo {
  float generated_font_size{};
  float ascent{};
  float descent{};
  float line_gap{};
  int associated_bitmap_width{};
  int associated_bitmap_height{};
  std::vector<stbtt_packedchar> stb_packed_chars;
  std::unordered_map<int, int> code_points_to_packed_chars_index;
  std::vector<int> range_offsets;
};

} //  font

namespace {

using namespace font;

Optional<int> find_packed_char_index(const FontInfo* info, int code_point) {
  const auto& cp_map = info->code_points_to_packed_chars_index;
  auto cp_it = cp_map.find(code_point);
  if (cp_it == cp_map.end()) {
    return NullOpt{};
  }
  return Optional<int>(cp_it->second);
}

bool has_code_point(const FontInfo* info, int code_point) {
  return bool(find_packed_char_index(info, code_point));
}

stbtt_aligned_quad get_stbtt_packed_quad(const FontInfo* info, int pc_index, float* x, float* y) {
  constexpr int align_to_integer = 0;
  stbtt_aligned_quad quad{};
  stbtt_GetPackedQuad(
    info->stb_packed_chars.data(),
    info->associated_bitmap_width,
    info->associated_bitmap_height, pc_index,
    x, y, &quad, align_to_integer);
  return quad;
}

FontBitmapSampleInfo
get_font_bitmap_sample_info(const FontInfo* info, int pc_index, float font_size, float* x, float* y) {
  const auto quad = get_stbtt_packed_quad(info, pc_index, x, y);
  const float font_scale = font_size / info->generated_font_size;
  FontBitmapSampleInfo dst_quad{};  //  @TODO: Get bitmap index.
  dst_quad.x0 = quad.x0 * font_scale;
  dst_quad.x1 = quad.x1 * font_scale;
  dst_quad.y0 = quad.y0 * font_scale;
  dst_quad.y1 = quad.y1 * font_scale;
  dst_quad.u0 = quad.s0;
  dst_quad.u1 = quad.s1;
  dst_quad.v0 = quad.t0;
  dst_quad.v1 = quad.t1;
  return dst_quad;
}

void sequence_size(const FontInfo* info, const int* code_points, int num_cps,
                   float font_size, float* x, float* width, float* end_pos) {
  const float font_scale = font_size / info->generated_font_size;

  const float x0 = *x;
  float y{};
  float p{};
  for (int i = 0; i < num_cps; i++) {
    if (auto pc_ind = find_packed_char_index(info, code_points[i])) {
      auto q0 = get_stbtt_packed_quad(info, pc_ind.value(), x, &y);
      p = q0.x1 * font_scale;
    }
  }

  *width = (*x - x0) * font_scale;
  *end_pos = p;
}

float sequence_width(const FontInfo* info, const int* code_points, int num_cps, float font_size) {
  float x{};
  float end{};
  float width{};
  sequence_size(info, code_points, num_cps, font_size, &x, &width, &end);
  return width;
}

int word_end(const FontInfo* info, const int* code_points, int num_code_points, int i, int delim) {
  if (i < num_code_points) {
    if (!has_code_point(info, code_points[i])) {
      //  If the first code point doesn't exist, the end of the "word" is the first code point that
      //  does exist.
      while (i < num_code_points && !has_code_point(info, code_points[i])) {
        i++;
      }
    } else if (code_points[i] == delim) {
      //  If the first code point is the word delimiter, the end of the "word" is the first
      //  non-delimiter code point that exists.
      while (i < num_code_points) {
        if (has_code_point(info, code_points[i]) && code_points[i] != delim) {
          break;
        }
        i++;
      }
    } else {
      //  The end of the word is the first delimiter.
      while (i < num_code_points) {
        if (code_points[i] == delim) {
          break;
        }
        i++;
      }
    }
  }
  return i;
}

} //  anon

font::FontInfo* font::create_font_bitmaps(const unsigned char* ttf_data,
                                          const CreateFontBitmapParams& params) {
  assert(params.font_size_px > 0.0f);

  int tot_num_points{};
  for (int i = 0; i < params.num_code_point_ranges; i++) {
    tot_num_points += params.code_point_ranges[i].size;
  }
  assert(tot_num_points > 0);

  auto* result = new FontInfo();
  result->stb_packed_chars.resize(tot_num_points);
  result->range_offsets.resize(params.num_code_point_ranges);

  stbtt_GetScaledFontVMetrics(
    ttf_data, 0, params.font_size_px, &result->ascent, &result->descent, &result->line_gap);
  result->generated_font_size = params.font_size_px;
  result->associated_bitmap_width = params.image_width;
  result->associated_bitmap_height = params.image_height;

  int code_point_offset{};
  for (int i = 0; i < params.num_code_point_ranges; i++) {
    auto& cp_range = params.code_point_ranges[i];
    unsigned char* dst_data = params.dst_image_data[i];

    const int w = params.image_width;
    const int h = params.image_height;

    stbtt_pack_context pack_context;
    stbtt_PackBegin(&pack_context, dst_data, w, h, 0, 1, nullptr);

    if (params.oversampling > 0) {
      stbtt_PackSetOversampling(&pack_context, params.oversampling, params.oversampling);
    }

    stbtt_pack_range pack_range{};
    pack_range.array_of_unicode_codepoints = cp_range.data;
    pack_range.num_chars = cp_range.size;
    pack_range.font_size = params.font_size_px;
    pack_range.chardata_for_range = result->stb_packed_chars.data() + code_point_offset;

    stbtt_PackFontRanges(&pack_context, ttf_data, 0, &pack_range, 1);
    stbtt_PackEnd(&pack_context);

    for (int j = 0; j < cp_range.size; j++) {
      const int cp = cp_range.data[j];
      assert(result->code_points_to_packed_chars_index.count(cp) == 0);
      result->code_points_to_packed_chars_index[cp] = j + code_point_offset;
    }

    result->range_offsets[i] = code_point_offset;
    code_point_offset += cp_range.size;
  }

  return result;
}

void font::destroy_font_info(FontInfo** info) {
  delete *info;
  *info = nullptr;
}

int font::gen_bitmap_samples(const FontInfo* info, const int* code_points, int num_code_points,
                             float font_size, FontBitmapSampleInfo* dst, float* xoff, float* yoff) {
  float x = xoff ? *xoff : 0.0f;
  float y = info->descent + info->generated_font_size + (yoff ? *yoff : 0.0f);

  int num_generated{};
  for (int i = 0; i < num_code_points; i++) {
    if (auto pc_ind = find_packed_char_index(info, code_points[i])) {
      dst[num_generated++] = get_font_bitmap_sample_info(info, pc_ind.value(), font_size, &x, &y);
    }
  }

  if (xoff) {
    *xoff = x;
  }
  if (yoff) {
    *yoff = y - (info->descent + info->generated_font_size);
  }

  return num_generated;
}

int font::gen_bitmap_samples_left_justify(
  const FontInfo* info, const int* code_points, int num_code_points,
  float font_size, float max_width, int delim, FontBitmapSampleInfo* dst, float* xoff, float* yoff) {
  //
  if (num_code_points == 0) {
    return 0;
  }

  float x = xoff ? *xoff : 0.0f;
  float y = info->descent + info->generated_font_size + (yoff ? *yoff : 0.0f);

  int i{};
  int num_generated{};
  while (i < num_code_points) {
    int i1 = word_end(info, code_points, num_code_points, i, delim);
    assert(i1 > i);

    if (x == 0.0f && code_points[i] == delim) {
      //  Skip leading delimiter word.
      i = i1;
      continue;
    }

    float end_pos{};
    float width{};
    float tx = x;
    sequence_size(info, code_points + i, i1 - i, font_size, &tx, &width, &end_pos);

    if (end_pos > max_width && x != 0.0f) {
      //  Word extends past `max_width` bounds.
      y += info->generated_font_size;
      x = 0.0f;
      if (code_points[i] == delim) {
        continue;
      }
    }

    for (; i < i1; i++) {
      auto pc_ind = find_packed_char_index(info, code_points[i]);
      if (!pc_ind) {
        continue;
      }

      auto quad = get_font_bitmap_sample_info(info, pc_ind.value(), font_size, &x, &y);
      if (quad.x1 > max_width) {
        //  Split on next line.
        y += info->generated_font_size;
        x = 0.0f;
        quad = get_font_bitmap_sample_info(info, pc_ind.value(), font_size, &x, &y);
      }

      dst[num_generated++] = quad;
    }
  }

  if (xoff) {
    *xoff = x;
  }
  if (yoff) {
    *yoff = y - (info->descent + info->generated_font_size);
  }

  return num_generated;
}

void font::offset_bitmap_samples(FontBitmapSampleInfo* samples, int num_samples, float x, float y) {
  for (int i = 0; i < num_samples; i++) {
    auto& sample = samples[i];
    sample.x0 += x;
    sample.x1 += x;
    sample.y0 += y;
    sample.y1 += y;
  }
}

int font::clip_bitmap_samples(FontBitmapSampleInfo* in_out, int num_src,
                              float x0, float y0, float x1, float y1) {
  int num_generated{};
  for (int i = 0; i < num_src; i++) {
    auto& sample = in_out[i];

    if (sample.x0 >= x1 || sample.x1 <= x0 ||
        sample.y0 >= y1 || sample.y1 <= y0) {
      continue;
    }

    float x0_plus = std::max(0.0f, x0 - sample.x0);   //  + this amount
    float x1_minus = std::max(0.0f, sample.x1 - x1);  //  - this amount
    float y0_plus = std::max(0.0f, y0 - sample.y0);   //  + this amount
    float y1_minus = std::max(0.0f, sample.y1 - y1);  //  - this amount

    float x_span = sample.x1 - sample.x0;
    float u_span = sample.u1 - sample.u0;
    float u0_plus = x_span == 0.0f ? 0.0f : (x0_plus / x_span) * u_span;
    float u1_minus = x_span == 0.0f ? 0.0f : (x1_minus / x_span) * u_span;

    float y_span = sample.y1 - sample.y0;
    float v_span = sample.v1 - sample.v0;
    float v0_plus = y_span == 0.0f ? 0.0f : (y0_plus / y_span) * v_span;
    float v1_minus = y_span == 0.0f ? 0.0f : (y1_minus / y_span) * v_span;

    sample.x0 += x0_plus;
    sample.x1 -= x1_minus;
    sample.y0 += y0_plus;
    sample.y1 -= y1_minus;
    sample.u0 += u0_plus;
    sample.u1 -= u1_minus;
    sample.v0 += v0_plus;
    sample.v1 -= v1_minus;
    in_out[num_generated++] = sample;
  }

  return num_generated;
}

float font::get_sequence_width(const FontInfo* info, const int* code_points,
                               int num_code_points, float font_size) {
  return sequence_width(info, code_points, num_code_points, font_size);
}

GROVE_NAMESPACE_END
