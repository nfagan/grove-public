#include "image.hpp"
#include "grove/visual/Image.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <stb_image.h>
#include <stb_image_write.h>
#include <cstring>
#include <string>

GROVE_NAMESPACE_BEGIN

namespace {

[[maybe_unused]] constexpr const char* logging_id() {
  return "load/image";
}

} //  anon

template <typename T>
struct LoadFunction {
  //
};

template<>
struct LoadFunction<uint8_t> {
  static uint8_t* load(const char* file_path, int* width, int* height, int* num_components) {
    return stbi_load(file_path, width, height, num_components, 0);
  }
};

template<>
struct LoadFunction<float> {
  static float* load(const char* file_path, int* width, int* height, int* num_components) {
    return stbi_loadf(file_path, width, height, num_components, 0);
  }
};

template <typename T>
Image<T> load_image_impl(const char* file_path, bool* success, bool flip_y_on_load) {
  *success = false;

  int width;
  int height;
  int num_components;

  if (flip_y_on_load) {
    stbi_set_flip_vertically_on_load(true);
  }

  T* data = LoadFunction<T>::load(file_path, &width, &height, &num_components);

  if (flip_y_on_load) {
    stbi_set_flip_vertically_on_load(false);
  }

  if (!data) {
#if GROVE_LOGGING_ENABLED == 1
    std::string msg{"Failed to load image: "};
    msg += file_path;
    GROVE_LOG_ERROR_CAPTURE_META(msg.c_str(), logging_id());
#endif
    return Image<T>();
  }

  const std::size_t data_size = width * height * num_components;
  auto data_copy = std::make_unique<T[]>(data_size);
  std::memcpy(data_copy.get(), data, data_size * sizeof(T));
  stbi_image_free(data);

  *success = true;
  return Image<T>(std::move(data_copy), width, height, num_components);
}

Image<uint8_t> load_image(const char* file_path, bool* success, bool flip_y_on_load) {
  return load_image_impl<uint8_t>(file_path, success, flip_y_on_load);
}

Image<float> load_imagef(const char* file_path, bool* success, bool flip_y_on_load) {
  return load_image_impl<float>(file_path, success, flip_y_on_load);
}

bool write_image(const uint8_t* data, int w, int h, int num_components, const char* file_path,
                 bool flip_y_on_save) {
  if (flip_y_on_save) {
    stbi_flip_vertically_on_write(true);
  }

  auto res = stbi_write_png(file_path, w, h, num_components, data, 0);
  bool success = res == 1;

  if (flip_y_on_save) {
    stbi_flip_vertically_on_write(false);
  }

  return success;
}

GROVE_NAMESPACE_END
