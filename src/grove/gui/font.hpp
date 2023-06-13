#pragma once

namespace grove::font {

struct FontInfo;

struct FontBitmapSampleInfo {
  int bitmap_index;
  float x0;
  float y0;
  float x1;
  float y1;
  float u0;
  float u1;
  float v0;
  float v1;
};

struct CodePointRange {
  int* data;  //  array of code points
  int size;
};

struct CreateFontBitmapParams {
  float font_size_px;
  int image_width;
  int image_height;
  int oversampling;
  CodePointRange* code_point_ranges;
  int num_code_point_ranges;
  unsigned char** dst_image_data; //  1 image per point range
};

FontInfo* create_font_bitmaps(const unsigned char* ttf_data, const CreateFontBitmapParams& params);
void destroy_font_info(FontInfo** info);

float get_sequence_width(const FontInfo* info, const int* code_points, int num_code_points,
                         float font_size);

int gen_bitmap_samples_left_justify(const FontInfo* info, const int* code_points, int num_code_points,
                                    float font_size, float max_width, int delim,
                                    FontBitmapSampleInfo* dst, float* xoff, float* yoff);

int gen_bitmap_samples(const FontInfo* info, const int* code_points, int num_code_points,
                       float font_size, FontBitmapSampleInfo* dst, float* xoff, float* yoff);

void offset_bitmap_samples(FontBitmapSampleInfo* samples, int num_samples, float x, float y);
int clip_bitmap_samples(FontBitmapSampleInfo* in_out, int num_original_samples,
                        float x0, float y0, float x1, float y1);

}