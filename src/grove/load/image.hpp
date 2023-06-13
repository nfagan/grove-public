#pragma once

#include <cstdint>

namespace grove {
template <typename T>
struct Image;

Image<uint8_t> load_image(const char* file_path, bool* success, bool flip_y_on_load = false);
Image<float> load_imagef(const char* file_path, bool* success, bool flip_y_on_load = false);

bool write_image(const uint8_t* image,
                 int w, int h, int num_components,
                 const char* file_path,
                 bool flip_y_on_save = false);
}
