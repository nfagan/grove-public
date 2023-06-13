#pragma once

#include <vector>
#include <string>
#include "grove/visual/Image.hpp"

namespace grove {

namespace image {
struct Descriptor;
}

using NoiseTexture3Float = std::vector<Image<float>>;
using NoiseTexture3UInt8 = std::vector<Image<uint8_t>>;

NoiseTexture3Float read_3d_noise_texture(const char* file_path, bool* success);
NoiseTexture3UInt8 texture3_data_to_uint8(const NoiseTexture3Float& source);

std::unique_ptr<uint8_t[]>
read_3d_image_texture(const std::vector<std::string>& file_paths,
                      bool* success, int* width, int* height, int* depth,
                      int* num_components, bool flip_y);

template <typename T>
std::unique_ptr<T[]> pack_texture_layers(const std::vector<Image<T>>& images) {
  if (images.empty()) {
    return nullptr;
  }

  auto width0 = images[0].width;
  auto height0 = images[0].height;
  auto components0 = images[0].num_components_per_pixel;
  auto num_images = int(images.size());

  for (int i = 1; i < num_images; i++) {
    if (images[i].width != width0 ||
        images[i].height != height0 ||
        images[i].num_components_per_pixel != components0) {
      return nullptr;
    }
  }

  size_t num_elements_per_image = width0 * height0 * components0;
  size_t num_elements = num_elements_per_image * num_images;

  auto data = std::make_unique<T[]>(num_elements);
  auto* dest = data.get();
  size_t offset{0};

  for (auto& image : images) {
    auto* src = image.data.get();
    std::copy(src, src + num_elements_per_image, dest + offset);
    offset += num_elements_per_image;
  }

  return data;
}

bool read_3d_image(const std::vector<std::string>& file_paths, std::unique_ptr<uint8_t[]>* data,
                   image::Descriptor* image_desc);

}