#include "../../ornament_texture.hpp"
#include "grove/load/image.hpp"
#include "grove/visual/Image.hpp"
#include "grove/visual/image_process.hpp"
#include "grove/visual/types.hpp"
#include "grove/common/Optional.hpp"
#include "grove/common/Stopwatch.hpp"
#include "grove/common/algorithm.hpp"
#include "grove/math/random.hpp"
#include <cstdio>
#include <string>
#include <fstream>

using namespace grove;

namespace {

template <typename T>
void write_image(std::fstream& file, int rows, int cols, int nc, const T* data) {
  file.write((char*) &rows, sizeof(int));
  file.write((char*) &cols, sizeof(int));
  file.write((char*) &nc, sizeof(int));
  file.write((char*) data, rows * cols * nc * sizeof(T));
}

bool write_float_image(const char* file_p, int rows, int cols, int channels, float* data) {
  std::fstream file;
  file.open(file_p, std::ios_base::out | std::ios_base::binary);
  if (!file.good()) {
    return false;
  } else {
    write_image<float>(file, rows, cols, channels, data);
    return true;
  }
}

Optional<Image<uint8_t>> load_image(const char* im_p) {
  bool success;
  auto im = grove::load_image(im_p, &success);
  if (!success) {
    return NullOpt{};
  } else {
    return Optional<Image<uint8_t>>(std::move(im));
  }
}

Optional<Image<uint8_t>> load_image_to_median_filter() {
//  auto im_p = std::string{GROVE_PLAYGROUND_OUT_DIR} + "/fall-scene-bw.png";
  auto im_p = std::string{GROVE_PLAYGROUND_OUT_DIR} + "/fall-scene.png";
  return load_image(im_p.c_str());
}

void randn(float* out, int count) {
  for (int i = 0; i < count; i++) {
    *out++ = urandf();
  }
}

template <typename T>
struct Average2 {};

template <>
struct Average2<float> {
  static float evaluate(float a, float b) {
    if (b < a) {
      std::swap(a, b);
    }
    return (b - a) * 0.5f + a;
  }
};

template <typename T>
T reference_median(T* beg, T* end) {
  auto sz = end - beg;
  if (sz == 0) {
    return T(0);
  }
  std::sort(beg, end);
  auto* mid = beg + sz/2;
  if (sz % 2 == 1) {
    return *mid;
  } else {
    assert(sz > 1);
    auto* prev = mid - 1;
    return Average2<T>::evaluate(*mid, *prev);
  }
}

template <typename T>
T quick_select_median(T* beg, T* end) {
  auto sz = end - beg;
  if (sz == 0) {
    return T(0);
  }

  if (sz % 2 == 1) {
    return *alg::quick_select_in_place(beg, end, int((sz + 1) / 2));
  } else {
    auto k0 = *alg::quick_select_in_place(beg, end, int(sz / 2));
    auto k1 = *alg::quick_select_in_place(beg, end, int(sz / 2 + 1));
    return Average2<T>::evaluate(k0, k1);
  }
}

template <typename T>
void validate_quick_select_result(const T* qs_begin, const T* qs_end, const T* qs_pivot,
                                  T* tmp, int k) {
  auto s = qs_end - qs_begin;
  assert(s >= k);

  std::copy(qs_begin, qs_end, tmp);
  std::sort(tmp, tmp + s);
  assert(tmp[k-1] == *qs_pivot);
}

void test_quick_select() {
  std::vector<float> src;
  src.resize(1);
  src[0] = urandf();
  auto r = alg::quick_select_in_place(src.data(), src.data() + src.size(), 1);
  assert(r == src.data());

  std::vector<float> tmp;
  for (int i = 0; i < 10000; i++) {
    int rand_size = std::max(1, int(urand() * 1000.0));
    src.resize(rand_size);
    tmp.resize(src.size());
    int k = std::max(1, std::min(int(urandf() * 32.0f), int(src.size())));
    auto* p = alg::quick_select_in_place(src.data(), src.data() + src.size(), k);
    validate_quick_select_result(src.data(), src.data() + src.size(), p, tmp.data(), k);
  }
}

void compare_median_methods() {
  std::vector<float> src;
  std::vector<float> tmp;
  for (int i = 0; i < 100; i++) {
    tmp.resize(int(urand() * 1000.0));
    src.resize(tmp.size());
    randn(src.data(), int(src.size()));
    memcpy(tmp.data(), src.data(), src.size() * sizeof(float));
    auto qs_med = quick_select_median(tmp.data(), tmp.data() + tmp.size());

    memcpy(tmp.data(), src.data(), src.size() * sizeof(float));
    auto ref_med = reference_median(tmp.data(), tmp.data() + tmp.size());
    assert(ref_med == qs_med);
  }
}

void make_images() {
  const int rows = 512;
  const int cols = 512;

  auto shape_res = image::petal_shape1_pipeline(image::PetalShape1Params::make_debug1());

  std::vector<float> shape;
  std::vector<float> distance;
  std::vector<int> transform_index;
  image::make_default_line_distance_mask(
    shape_res, rows, cols, &shape, &distance, &transform_index);

  for (float d : distance) {
    assert(std::isfinite(d) && d >= 0.0f);
    (void) d;
  }

  std::vector<float> line_splotch_mask;
  image::make_default_line_splotch_mask(
    image::LineSplotchMaskParams::make_default(), rows, cols, &line_splotch_mask);
  for (float d : line_splotch_mask) {
    assert(std::isfinite(d) && d >= 0.0f && d <= 1.0f);
    (void) d;
  }

  const std::string dst_p{GROVE_PLAYGROUND_OUT_DIR};
  {
    auto shape_file_p = dst_p + "/shape.dat";
    auto dist_file_p = dst_p + "/dist.dat";
    auto line_file_p = dst_p + "/line_splotch.dat";
    if (!write_float_image(shape_file_p.c_str(), rows, cols, 1, shape.data())) {
      return;
    }
    if (!write_float_image(dist_file_p.c_str(), rows, cols, 1, distance.data())) {
      return;
    }
    if (!write_float_image(line_file_p.c_str(), rows, cols, 1, line_splotch_mask.data())) {
      return;
    }
  }

  if (auto maybe_im = load_image_to_median_filter()) {
    auto& im = maybe_im.value();
    const int n = 31;
//    const int n = 5;
    std::vector<uint8_t> dst_im(im.size());
    const int nc = im.num_components_per_pixel;

    Stopwatch stopwatch;
    image::median_filter_per_dimension_uint8n(
      im.data.get(), im.height, im.width, nc, n, true, dst_im.data(), true);
    printf("Computed with window size %d in %0.2fs\n", n, float(stopwatch.delta().count()));

    auto filt_p = dst_p + "/median_filter.png";
    grove::write_image(
      dst_im.data(), im.width, im.height, im.num_components_per_pixel, filt_p.c_str());
  }
}

std::vector<uint8_t> to_uint8(const std::vector<float>& src, float multiplier) {
  std::vector<uint8_t> result(src.size());
  for (size_t i = 0; i < result.size(); i++) {
    result[i] = image::DefaultFloatConvert<uint8_t>::from_float(src[i] * multiplier);
  }
  return result;
}

std::vector<float> to_float(const Image<uint8_t>& im) {
  std::vector<float> res(im.size());
  for (size_t i = 0; i < im.size(); i++) {
    res[i] = float(im.data[i]) / 255.0f;
  }
  return res;
}

void median_filter_per_dim(const float* src, const image::Descriptor& desc, int n, float* dst) {
  image::median_filter_per_dimension_floatn(
    src, desc.rows(), desc.cols(), desc.num_channels(), n, false, dst);
}

#define MED_FILTER_IMAGES (0)

void test_material1() {
#if MED_FILTER_IMAGES
  const auto fall_p = std::string{GROVE_PLAYGROUND_OUT_DIR} + "/fall-scene-bw.png";
  const auto call_p = std::string{GROVE_PLAYGROUND_OUT_DIR} + "/calla_leaves-bw.png";
#else
  const auto fall_p = std::string{GROVE_PLAYGROUND_OUT_DIR} + "/fall-scene-bw-filt.png";
  const auto call_p = std::string{GROVE_PLAYGROUND_OUT_DIR} + "/calla_leaves-bw-filt.png";
#endif
  const auto dst_dir = std::string{GROVE_PLAYGROUND_OUT_DIR};

  auto fall_im = load_image(fall_p.c_str());
  if (!fall_im) {
    return;
  }
  auto fall_im_srcf = to_float(fall_im.value());
  auto fall_imf = fall_im_srcf;
  auto fall_desc = image::Descriptor::make_2d_floatn(
    fall_im.value().width, fall_im.value().height, fall_im.value().num_components_per_pixel);

  auto call_im = load_image(call_p.c_str());
  if (!call_im) {
    return;
  }
  auto call_im_srcf = to_float(call_im.value());
  auto call_imf = call_im_srcf;
  auto call_desc = image::Descriptor::make_2d_floatn(
    call_im.value().width, call_im.value().height, call_im.value().num_components_per_pixel);

#if MED_FILTER_IMAGES
  median_filter_per_dim(call_im_srcf.data(), call_desc, 31, call_imf.data());
  median_filter_per_dim(fall_im_srcf.data(), fall_desc, 31, fall_imf.data());
#endif

  const auto shape_res = image::petal_shape1_pipeline(image::PetalShape1Params::make_debug1());

  const int src_rows = 512;
  const int src_cols = 512;
  std::vector<float> shape;
  std::vector<float> distance;
  std::vector<int> transform_index;
  std::vector<float> line_splotch_mask;
  image::make_default_line_distance_mask(
    shape_res, src_rows, src_cols, &shape, &distance, &transform_index);
  image::make_default_line_splotch_mask(
    image::LineSplotchMaskParams::make_default(), src_rows, src_cols, &line_splotch_mask);

  const auto src_float1_desc = image::Descriptor::make_2d_floatn(src_cols, src_rows, 1);
  const auto src_int321_desc = image::Descriptor::make_2d_int32n(src_cols, src_rows, 1);

  const int dst_rows = 256;
  const int dst_cols = 256;
  std::vector<float> dst(dst_rows * dst_cols * 4);
  const auto dst_desc = image::Descriptor::make_2d_floatn(dst_cols, dst_rows, 4);

  image::PetalTextureMaterial1Params params{};
  params.dst = dst.data();
  params.dst_desc = &dst_desc;
  params.petal_shape = shape.data();
  params.petal_shape_desc = &src_float1_desc;
  params.distance = distance.data();
  params.distance_desc = &src_float1_desc;
  params.distance_power = 5.0f;
  params.petal_set_index = transform_index.data();
  params.petal_set_desc = &src_int321_desc;
  params.base_color_mask = call_imf.data();
  params.base_color_desc = &call_desc;
  params.center_color_mask = line_splotch_mask.data();
  params.center_color_desc = &src_float1_desc;
  params.center_color_scale = 2.0f;
  params.center_base_mask = fall_imf.data();
  params.center_base_desc = &fall_desc;
  params.petal_transforms = shape_res.petal_transforms.data();
  params.num_petal_transforms = int(shape_res.petal_transforms.size());
  image::petal_texture_material1(params);

#if 1
  std::vector<float> dst_di(dst_cols * dst_rows);
  for (int c = 0; c < 4; c++) {
    for (int i = 0; i < dst_rows; i++) {
      for (int j = 0; j < dst_cols; j++) {
        auto src_ind = image::ij_to_linear(i, j, dst_cols, 4) + c;
        auto dst_ind = image::ij_to_linear(i, j, dst_cols, 1);
        dst_di[dst_ind] = dst[src_ind];
      }
    }
    auto dst_p = dst_dir + "/petal1_material-";
    dst_p += std::to_string(c);
    dst_p += ".dat";
    if (!write_float_image(dst_p.c_str(), dst_rows, dst_cols, 1, dst_di.data())) {
      return;
    }
  }

  auto mat_color0 = image::srgb_to_linear(Vec3f(139, 216, 225) / 255.0f);
  auto mat_color1 = image::srgb_to_linear(Vec3f(86, 171, 225) / 255.0f);
  auto mat_color2 = image::srgb_to_linear(Vec3f(242, 93, 149) / 255.0f);
  auto mat_color3 = image::srgb_to_linear(Vec3f(242, 131, 188) / 255.0f);
//  auto mat_color2 = image::srgb_to_linear(Vec3f(242, 234, 145) / 255.0f);
//  auto mat_color3 = image::srgb_to_linear(Vec3f(242, 230, 93) / 255.0f);

  std::vector<float> dst_color(dst.size());
  image::apply_petal_texture_material(
    dst.data(), dst_desc, mat_color0, mat_color1, mat_color2, mat_color3, true, dst_color.data());
  auto dst_color_u8 = to_uint8(dst_color, 255.0f);
  auto dst_color_p = dst_dir + "/petal1_material_color.png";
  grove::write_image(
    dst_color_u8.data(), dst_desc.width(), dst_desc.height(), dst_desc.num_channels(),
    dst_color_p.c_str());

  {
    auto dst_p = dst_dir + "/petal1_material.png";
    auto dst_info_u8 = to_uint8(dst, 255.0f);
    grove::write_image(
      dst_info_u8.data(),
      dst_desc.width(), dst_desc.height(),
      dst_desc.num_channels(), dst_p.c_str());
  }
#else
  auto dst_p = dst_dir + "/petal1_material.png";

  for (auto& f : dst) {
    f *= 255.0f;
  }
  auto res = to_uint8(dst);
  grove::write_image(
    res.data(), dst_desc.width(), dst_desc.height(), dst_desc.num_channels(), dst_p.c_str());
#endif
}

} //  anon

int main(int, char**) {
  test_material1();
//  test_quick_select();
//  compare_median_methods();
//  make_images();
  return 0;
}