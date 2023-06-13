#include "texture_io.hpp"
#include "grove/common/common.hpp"
#include "grove/math/util.hpp"
#include "grove/load/image.hpp"
#include "grove/visual/types.hpp"
#include <fstream>

GROVE_NAMESPACE_BEGIN

NoiseTexture3Float read_3d_noise_texture(const char* file_path, bool* success) {
  *success = false;

  std::ifstream file;
  file.open(file_path, std::ios_base::in | std::ios_base::binary);

  if (!file.good()) {
    return {};
  }

  file.seekg(0, file.end);
  const std::size_t file_size_bytes = file.tellg();
  file.seekg(0, file.beg);

  auto header_size_bytes = sizeof(int) * 2;

  if (file_size_bytes < header_size_bytes) {
    return {};
  }

  int num_layers;
  int texture_size;

  file.read((char*) &num_layers, sizeof(int));
  file.read((char*) &texture_size, sizeof(int));

  auto layer_size_bytes = texture_size * texture_size * sizeof(float);
  auto data_size_bytes = num_layers * layer_size_bytes;

  if (data_size_bytes + header_size_bytes != file_size_bytes) {
    return {};
  }

  NoiseTexture3Float result;

  for (int i = 0; i < num_layers; i++) {
    auto res = std::unique_ptr<float[]>(new float[texture_size * texture_size]);
    file.read((char*) res.get(), layer_size_bytes);
    result.emplace_back(std::move(res), texture_size, texture_size, 1);
  }

  *success = true;
  return result;
}

NoiseTexture3UInt8 texture3_data_to_uint8(const NoiseTexture3Float& source) {
  NoiseTexture3UInt8 result;

  for (auto& image : source) {
    auto num_elements = image.size();
    auto res = std::make_unique<uint8_t[]>(num_elements);

    for (size_t i = 0; i < num_elements; i++) {
      auto v = clamp(image.data[i], 0.0f, 1.0f);
      auto u = uint8_t(clamp(int(v * 255), 0, 255));
      res[i] = u;
    }

    result.emplace_back(
      std::move(res), image.width, image.height, image.num_components_per_pixel);
  }

  return result;
}

std::unique_ptr<uint8_t[]> read_3d_image_texture(const std::vector<std::string>& file_paths,
                                                 bool* success, int* width, int* height,
                                                 int* depth, int* num_components, bool flip_y) {
  if (file_paths.empty()) {
    *success = true;
    *width = 0;
    *height = 0;
    *depth = 0;
    *num_components = 0;
    return nullptr;
  }

  std::vector<Image<uint8_t>> images;
  for (auto& file : file_paths) {
    bool tmp_success;
    auto im = load_image(file.c_str(), &tmp_success, flip_y);
    if (!tmp_success) {
      *success = false;
      return nullptr;
    } else {
      images.push_back(std::move(im));
    }
  }

  if (auto packed = pack_texture_layers(images)) {
    *success = true;
    *width = images[0].width;
    *height = images[0].height;
    *depth = int(images.size());
    *num_components = images[0].num_components_per_pixel;
    return packed;

  } else {
    *success = false;
    return nullptr;
  }
}

bool read_3d_image(const std::vector<std::string>& file_paths, std::unique_ptr<uint8_t[]>* data,
                   image::Descriptor* image_desc) {
  bool success{};
  int width{};
  int height{};
  int depth{};
  int num_components{};
  bool flip_y = false;
  auto res = read_3d_image_texture(
    file_paths, &success, &width, &height, &depth, &num_components, flip_y);

  *data = std::move(res);
  *image_desc = image::Descriptor{
    image::Shape::make_3d(width, height, depth),
    image::Channels::make_uint8n(num_components)
  };

  return success;
}

GROVE_NAMESPACE_END