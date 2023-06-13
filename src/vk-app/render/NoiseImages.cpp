#include "NoiseImages.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

using namespace vk;

namespace {

void make_bayer_pattern(const uint8_t** out, int* w, int* h) {
  static const uint8_t pattern[] = {
    0, 32,  8, 40,  2, 34, 10, 42,
    48, 16, 56, 24, 50, 18, 58, 26,
    12, 44,  4, 36, 14, 46,  6, 38,
    60, 28, 52, 20, 62, 30, 54, 22,
    3, 35, 11, 43,  1, 33,  9, 41,
    51, 19, 59, 27, 49, 17, 57, 25,
    15, 47,  7, 39, 13, 45,  5, 37,
    63, 31, 55, 23, 61, 29, 53, 21
  };

  auto n = sizeof(pattern);
  assert(n == 64);
  (void) n;
  *out = pattern;
  *w = 8;
  *h = 8;
}

} //  anon

void vk::NoiseImages::initialize(const InitInfo& init_info) {
  {
    //  bayer8
    const uint8_t* data{};
    int w{};
    int h{};
    make_bayer_pattern(&data, &w, &h);

    SampledImageManager::ImageCreateInfo create_info{};
    create_info.image_type = SampledImageManager::ImageType::Image2D;
    create_info.descriptor = {
      image::Shape::make_2d(w, h),
      image::Channels::make_uint8n(1)
    };
    create_info.data = data;
    create_info.format = VK_FORMAT_R8_UNORM;
    create_info.sample_in_stages = {PipelineStage::FragmentShader};
    auto handle = init_info.image_manager.create_sync(create_info);
    if (handle) {
      bayer8 = handle.value();
    }
  }
}

GROVE_NAMESPACE_END

