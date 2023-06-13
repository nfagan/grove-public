#include "ornament_texture.hpp"
#include "grove/common/common.hpp"
#include "grove/common/algorithm.hpp"
#include "grove/math/constants.hpp"
#include "grove/math/window.hpp"
#include "grove/math/random.hpp"
#include "grove/math/matrix.hpp"
#include "grove/math/matrix_transform.hpp"
#include "grove/visual/image_process.hpp"
#include "grove/visual/types.hpp"
#include "grove/math/util.hpp"
#include "grove/audio/filter.hpp"
#include <numeric>

GROVE_NAMESPACE_BEGIN

namespace {

using namespace image;

Vec2f petal_transform(const Mat2f& rot, float scale, float offset, const Vec2f& p) {
  Vec2f v{rot(0, 1), rot(1, 1)};
  return rot * ((p - 0.5f) * scale) + 0.5f + v * offset;
}

Vec2f inverse_petal_transform(const Mat2f& rot, float scale, float offset, const Vec2f& pi) {
  Vec2f v{rot(0, 1), rot(1, 1)};
  auto p = pi;
  p = p - 0.5f - v * offset;
  p = inverse(rot) * p;
  p = p / scale;
  p = p + 0.5f;
  return p;
}

void distribute_rotated(int n, float rand_scale, float radial_offset,
                        float phase_offset, PetalTransform* dst) {
  for (int i = 0; i < n; i++) {
    const float scl = std::max(0.001f, 0.5f + urand_11f() * rand_scale);
    dst[i] = PetalTransform{2.0f * pif() / float(n) * float(i) + phase_offset, scl, radial_offset};
  }
}

void get_dim(const Vec2f* ps, int n, int dim, float* dst) {
  for (int i = 0; i < n; i++) {
    dst[i] = ps[i][dim];
  }
}

void set_dim(const float* src, int n, int dim, Vec2f* dst) {
  for (int i = 0; i < n; i++) {
    dst[i][dim] = src[i];
  }
}

void get_limits(const float* src, int n, float* mn, float* mx) {
  if (n > 0) {
    *mn = *std::min_element(src, src + n);
    *mx = *std::max_element(src, src + n);
  }
}

void apply_limits(float mn, float mx, int n, const float* src, float* dst) {
  const float span = mx - mn;
  for (int i = 0; i < n; i++) {
    dst[i] = (src[i] - mn) / span;
  }
}

void norm01(const float* src, int n, float* dst) {
  float mn;
  float mx;
  get_limits(src, n, &mn, &mx);
  apply_limits(mn, mx, n, src, dst);
}

void invert01(const float* src, int n, float* dst) {
  for (int i = 0; i < n; i++) {
    dst[i] = 1.0f - src[i];
  }
}

int to_int_window_size(float win_size, int rows, int cols) {
  int n{};
  if (win_size < 1.0f) {
    const int min_dim = std::min(rows, cols);
    n = std::max(1, int(std::floor(float(min_dim) * win_size)));
    n += ((n % 2) == 0 && n < min_dim) ? 1 : 0;
  } else {
    n = int(std::floor(win_size));
  }
  return n;
}

void make_smooth_noise1d(int n, int ng, float* tmp_b, float* tmp_x, float* dst) {
  win::gauss1d(tmp_b, ng);
  const float den = std::accumulate(tmp_b, tmp_b + ng, 0.0f);
  for (int i = 0; i < ng; i++) {
    tmp_b[i] /= den;
  }

  for (int i = 0; i < n; i++) {
    dst[i] = urandf();
  }

  std::fill(tmp_x, tmp_x + ng, 0.0f);
  const float tmp_a = 1.0f;
  float tmp_y = 0.0f;
  for (int i = 0; i < n; i++) {
    dst[i] = audio::linear_filter_tick(tmp_b, ng, &tmp_a, 1, tmp_x, &tmp_y, dst[i]);
  }
}

void make_sin_curve(int n, float x_scale, float y_scale, Vec2f* dst) {
  for (int i = 0; i < n; i++) {
    float y = (float(i) + 0.5f) / float(n);
    float th = y * pif();
    float x = std::sin(th) * x_scale + 0.5f;
    y = y * y_scale + (1.0f - y_scale) * 0.5f;
    dst[i] = Vec2f{x, y};
  }
}

void estimate_normals(const Vec2f* ps, int n, Vec2f* ns) {
  for (int i = 0; i < n-1; i++) {
    const auto& p0 = ps[i];
    const auto& p1 = ps[i+1];
    auto v = normalize(p1 - p0);
    ns[i] = Vec2f{-v.y, v.x};
  }
  if (n > 1) {
    ns[n - 1] = ns[n - 2];
  }
}

void reflect_x_ps(const Vec2f* src_ps, int n, Vec2f* dst_ps) {
  for (int i = 0; i < n; i++) {
    dst_ps[i] = Vec2f{1.0f - src_ps[i].x, src_ps[i].y};
  }
}

void reflect_x_ns(const Vec2f* src_ns, int n, Vec2f* dst_ns) {
  for (int i = 0; i < n; i++) {
    dst_ns[i] = Vec2f{-src_ns[i].x, src_ns[i].y};
  }
}

void offset_x(const Vec2f* src, const float* amt, int n,
              float filt_scale, float x_scale, Vec2f* dst) {
  for (int i = 0; i < n; i++) {
    dst[i] = src[i];
    float off = (amt[i] * 2.0f - 1.0f) * filt_scale;
    float dist_x = std::abs(dst[i].x - 0.5f) / x_scale;
    dst[i].x += off * dist_x;
  }
}

float sign_or_zero(float v) {
  return v == 0.0f ? 0.0f : v < 0.0f ? -1.0f : 1.0f;
}

auto line_distance(const Vec2f* ps, const Vec2f* ns, float* tmp, int n, const Vec2f& p) {
  struct Result {
    float v;
    float distance;
  };

  Result result{};

  for (int it = 0; it < n; it++) {
    tmp[it] = (ps[it] - p).length();
  }

  const auto* mn = std::min_element(tmp, tmp + n);
  if (n > 0) {
    const auto mn_ind = int(mn - tmp);
    auto& norm = ns[mn_ind];
    auto to_p = p - ps[mn_ind];
    auto sim = dot(norm, to_p);
    result.v = (sign_or_zero(sim) * 0.5f + 0.5f);
    result.distance = tmp[mn_ind];
  }

  return result;
}

void line_distance(const Vec2f* ps, const Vec2f* ns, float* tmp, int n, int rows, int cols,
                   float* dst_shape, float* dst_distance) {
  int li{};
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
      Vec2f p;
      ij_to_uv(i, j, rows, cols, &p.x, &p.y);
      auto dist_res = line_distance(ps, ns, tmp, n, p);
      dst_shape[li] = dist_res.v;
      dst_distance[li] = dist_res.distance;
      li++;
    }
  }
}

float oriented_line_distance(const Vec2f* p0, const Vec2f* p1, float* tmp, int n, const Vec2f& p) {
  int num_in_span{};
  float v{};

  for (int i = 0; i < n; i++) {
    if (p.x >= p0[i].x && p.x < p1[i].x) {
      float s = p1[i].x - p0[i].x;
      float fs = (p.x - p0[i].x) / s;
      assert(fs >= 0.0f && fs < 1.0f);

      float d_v0 = std::abs(p.y - p0[i].y);
      float d_v1 = std::abs(p.y - p1[i].y);
      float d_v = lerp(fs, d_v0, d_v1);
      v += d_v;
      tmp[num_in_span++] = d_v;
    }
  }

  if (num_in_span > 0) {
    v = *std::min_element(tmp, tmp + num_in_span);
  }

  return v;
}

void oriented_line_distance(const Vec2f* p0, const Vec2f* p1, float* tmp, int n,
                            int rows, int cols, float* dst) {
  int li{};
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
      Vec2f p;
      ij_to_uv(i, j, rows, cols, &p.x, &p.y);
      dst[li++] = oriented_line_distance(p0, p1, tmp, n, p);
    }
  }
  norm01(dst, rows * cols, dst);
}

auto line_splotch_points(const LineSplotchMaskParams& params) {
  struct Result {
    std::vector<Vec2f> p0s;
    std::vector<Vec2f> p1s;
  };

  const int nf = params.num_line_points;
  const int ng = params.num_filter_points;
  const int reps = params.num_reps;
  const float space = params.space;
  const Vec2f y_space{0.0f, space};
  const Vec2f off = params.off;
  const float line_noise_scale = params.line_noise_scale;
  const float rot_frac = params.rot_frac;
  const float expand = params.expand;
  const float expand_off = params.expand_off;

  const auto rot = make_rotation(pif() * rot_frac);

  std::vector<float> line_noise(nf);
  std::vector<float> filt_tmp_b(ng);
  std::vector<float> filt_tmp_x(ng);

  std::vector<Vec2f> p0s(nf-1);
  std::vector<Vec2f> p1s(nf-1);

  std::vector<Vec2f> p0_tot;
  std::vector<Vec2f> p1_tot;

  for (int iter = 0; iter < reps; iter++) {
    make_smooth_noise1d(nf, ng, filt_tmp_b.data(), filt_tmp_x.data(), line_noise.data());

    Vec2f tot{};
    float tot_den{};
    for (int i = 0; i < nf-1; i++) {
      int p0_ind = i;
      int p1_ind = i + 1;
      auto p0 = Vec2f{float(p0_ind) / float(nf-1), line_noise[p0_ind] * line_noise_scale};
      auto p1 = Vec2f{float(p1_ind) / float(nf-1), line_noise[p1_ind] * line_noise_scale};
      p0s[i] = p0;
      p1s[i] = p1;
      tot += p0 + p1;
      tot_den += 2.0f;
    }

    const auto cent = tot / tot_den;
    const auto y_off = off + y_space * float(iter);
    for (int i = 0; i < nf-1; i++) {
      p0s[i] = rot * (p0s[i] - cent) + cent + y_off;
      p1s[i] = rot * (p1s[i] - cent) + cent + y_off;
      p0_tot.push_back(p0s[i]);
      p1_tot.push_back(p1s[i]);
    }
  }

  {
    std::vector<float> p0x(p0_tot.size());
    get_dim(p0_tot.data(), int(p0_tot.size()), 0, p0x.data());

    std::vector<float> p1x(p0_tot.size());
    get_dim(p1_tot.data(), int(p1_tot.size()), 0, p1x.data());

    float mnp0;
    float mxp0;
    get_limits(p0x.data(), int(p0x.size()), &mnp0, &mxp0);

    apply_limits(mnp0, mxp0, int(p0x.size()), p0x.data(), p0x.data());
    apply_limits(mnp0, mxp0, int(p1x.size()), p1x.data(), p1x.data());

    set_dim(p0x.data(), int(p0x.size()), 0, p0_tot.data());
    set_dim(p1x.data(), int(p1x.size()), 0, p1_tot.data());
  }

  for (auto& p : p0_tot) {
    p.x *= expand;
    p.x += expand_off;
  }

  for (auto& p : p1_tot) {
    p.x *= expand;
    p.x += expand_off;
  }

  Result result;
  result.p0s = std::move(p0_tot);
  result.p1s = std::move(p1_tot);
  return result;
}

auto line_distance_mask(const std::vector<std::vector<Vec2f>>& p_sets,
                        const std::vector<std::vector<Vec2f>>& n_sets, int rows, int cols) {
  struct Result {
    std::vector<float> shape;
    std::vector<float> distance;
    std::vector<int> set_index;
  };

  Result result;

  std::vector<float> result_shape(rows * cols);
  std::vector<float> result_distance(rows * cols);
  std::vector<int> result_set_index(rows * cols);

  if (!p_sets.empty()) {
    std::vector<float> tmp_shape(rows * cols);
    std::vector<float> tmp_distance(rows * cols);

    auto& ps = p_sets[0];
    auto& ns = n_sets[0];
    assert(ps.size() == ns.size());
    const int n = int(ps.size());
    std::vector<float> tmp(n);
    line_distance(
      ps.data(), ns.data(), tmp.data(), n, rows, cols, result_shape.data(), result_distance.data());

    for (int it = 1; it < int(p_sets.size()); it++) {
      auto& ps1 = p_sets[it];
      auto& ns1 = n_sets[it];
      assert(ps1.size() == ns1.size());
      const int n1 = int(ps1.size());
      tmp.resize(n1);
      line_distance(
        ps1.data(), ns1.data(), tmp.data(), n1, rows, cols, tmp_shape.data(), tmp_distance.data());

      for (int i = 0; i < rows * cols; i++) {
        if (tmp_shape[i] > result_shape[i]) {
          result_shape[i] = tmp_shape[i];
          result_distance[i] = tmp_distance[i];
          result_set_index[i] = it;
        }
      }
    }
  }

  result.shape = std::move(result_shape);
  result.distance = std::move(result_distance);
  result.set_index = std::move(result_set_index);
  return result;
}

} //  anon

PetalShape1Params image::PetalShape1Params::make_debug1() {
  PetalShape1Params result{};
  result.num_curve_pts = 32;
  result.filter_win_size = 10;
  result.filter_noise_scale = 0.05f;
  result.x_scale = 0.25f;
  result.y_scale = 0.75f;
  result.petal_rand_scale = 0.05f;
  result.petal_radial_off = 0.15f;
  result.petal_phase_off = 0.0f;
  result.num_petals = 6;
  return result;
}

LineSplotchMaskParams image::LineSplotchMaskParams::make_default() {
  LineSplotchMaskParams result{};
  result.num_line_points = 32;
  result.num_filter_points = 10;
  result.num_reps = 64;
  result.space = 0.02f;
  result.off = Vec2f{0.0f, -0.15f};
  result.line_noise_scale = 0.075f;
  result.rot_frac = 0.1f;
  result.expand = 1.2f;
  result.expand_off = -0.1f;
  return result;
}

PetalShape1Result image::petal_shape1_pipeline(const PetalShape1Params& params) {
  const int num_curve_pts = params.num_curve_pts;
  const float x_scale = params.x_scale;
  const float y_scale = params.y_scale;

  std::vector<Vec2f> ps(num_curve_pts);
  std::vector<Vec2f> ns(num_curve_pts);
  make_sin_curve(num_curve_pts, x_scale, y_scale, ps.data());

  {
    std::vector<float> filt1(num_curve_pts);
    std::vector<float> filt_tmp_b(params.filter_win_size);
    std::vector<float> filt_tmp_x(params.filter_win_size);
    make_smooth_noise1d(
      num_curve_pts, params.filter_win_size, filt_tmp_b.data(), filt_tmp_x.data(), filt1.data());
    offset_x(ps.data(), filt1.data(), num_curve_pts, params.filter_noise_scale, x_scale, ps.data());
  }

  estimate_normals(ps.data(), num_curve_pts, ns.data());

  //  reflect
  ps.resize(num_curve_pts * 2);
  ns.resize(num_curve_pts * 2);

  reflect_x_ps(ps.data(), num_curve_pts, ps.data() + num_curve_pts);
  reflect_x_ns(ns.data(), num_curve_pts, ns.data() + num_curve_pts);

  std::vector<PetalTransform> petal_transforms(params.num_petals);
  distribute_rotated(
    params.num_petals,
    params.petal_rand_scale,
    params.petal_radial_off,
    params.petal_phase_off,
    petal_transforms.data());

  std::vector<std::vector<Vec2f>> p_sets;
  std::vector<std::vector<Vec2f>> n_sets;

  for (const PetalTransform& pt : petal_transforms) {
    auto& dst_ps = p_sets.emplace_back();
    auto& dst_ns = n_sets.emplace_back();

    dst_ps.resize(ps.size());
    dst_ns.resize(ns.size());

    const Mat2f rot = make_rotation(pt.theta);
    for (size_t i = 0; i < ps.size(); i++) {
      dst_ps[i] = petal_transform(rot, pt.scale, pt.offset, ps[i]);
      dst_ns[i] = rot * ns[i];
    }
  }

  PetalShape1Result result;
  result.p_sets = std::move(p_sets);
  result.n_sets = std::move(n_sets);
  result.petal_transforms = std::move(petal_transforms);
  return result;
}

void image::make_default_line_distance_mask(const PetalShape1Result& shape_result,
                                            int rows, int cols,
                                            std::vector<float>* shape,
                                            std::vector<float>* distance,
                                            std::vector<int>* set_index) {
  auto dist_mask = line_distance_mask(shape_result.p_sets, shape_result.n_sets, rows, cols);

  auto& dist = dist_mask.distance;
  norm01(dist.data(), int(dist.size()), dist.data());
  invert01(dist.data(), int(dist.size()), dist.data());

  auto& shape_im = dist_mask.shape;
  norm01(shape_im.data(), int(shape_im.size()), shape_im.data());

  auto tmp_shape = shape_im;
  const int gauss_n = to_int_window_size(0.022f, rows, cols);
  const float gauss_s = 3.0f;
  std::vector<float> gauss_h(gauss_n * gauss_n);
  win::gauss2d(gauss_h.data(), gauss_n, gauss_s, true);
  image::xcorr(tmp_shape.data(), rows, cols, gauss_h.data(), gauss_n, true, shape_im.data());

  *shape = std::move(dist_mask.shape);
  *distance = std::move(dist_mask.distance);
  *set_index = std::move(dist_mask.set_index);
}

void image::make_default_line_splotch_mask(const LineSplotchMaskParams& params,
                                           int rows, int cols, std::vector<float>* mask) {
  *mask = std::vector<float>(rows * cols);

  auto pts = line_splotch_points(params);
  std::vector<float> tmp(pts.p0s.size());
  oriented_line_distance(
    pts.p0s.data(), pts.p1s.data(), tmp.data(), int(tmp.size()), rows, cols, mask->data());

  //  mask = (1 - mask) .^ 3
  for (auto& f : *mask) {
    f = 1.0f - f;
    f = f * f * f;
  }

  //  mask = median_filter(mask)
  const auto filt_size = to_int_window_size(0.08f, rows, cols);
  std::vector<float> tmp_filt(mask->size());
  memcpy(tmp_filt.data(), mask->data(), mask->size() * sizeof(float));
  median_filter_per_dimension_floatn(tmp_filt.data(), rows, cols, 1, filt_size, false, mask->data());
}

void image::petal_texture_material1(const PetalTextureMaterial1Params& params) {
  assert(params.petal_set_desc->num_channels() == 1);
  assert(params.base_color_desc->num_channels() == 1);
  assert(params.center_color_desc->num_channels() == 1);
  assert(params.center_base_desc->num_channels() == 1);
  assert(params.distance_desc->num_channels() == 1);
  assert(params.petal_shape_desc->num_channels() == 1);
  assert(params.dst_desc->num_channels() == 4);
  assert(params.distance_power > 0.0f);

  for (int i = 0; i < params.dst_desc->rows(); i++) {
    for (int j = 0; j < params.dst_desc->cols(); j++) {
      const auto uv = grove::ij_to_uv(i, j, *params.dst_desc);

      int inv_trans_ind;
      image::sample_nearest(params.petal_set_index, *params.petal_set_desc, uv, &inv_trans_ind);
      assert(inv_trans_ind < params.num_petal_transforms);
      const auto& tform = params.petal_transforms[inv_trans_ind];
      auto inv_uv = inverse_petal_transform(
        make_rotation(tform.theta), tform.scale, tform.offset, uv);

      const auto& base_col_uv = inv_uv;
      float base_col;
      image::sample_bilinear(
        params.base_color_mask, *params.base_color_desc, base_col_uv, &base_col);

      const auto& center_col_uv = inv_uv;
      float center_col;
      image::sample_bilinear(
        params.center_color_mask, *params.center_color_desc, center_col_uv, &center_col);
      center_col = clamp(center_col * params.center_color_scale, 0.0f, 1.0f);

      const auto& center_base_uv = inv_uv;
      float center_base;
      image::sample_bilinear(
        params.center_base_mask, *params.center_base_desc, center_base_uv, &center_base);

      const auto& dist_uv = uv;
      float dist_val;
      image::sample_bilinear(params.distance, *params.distance_desc, dist_uv, &dist_val);
      dist_val = std::pow(std::max(dist_val, 0.0f), params.distance_power);

      const auto& shape_mask_uv = uv;
      float shape_mask_val;
      image::sample_bilinear(
        params.petal_shape, *params.petal_shape_desc, shape_mask_uv, &shape_mask_val);

      const int dst_ind = ij_to_linear(i, j, *params.dst_desc);
      params.dst[dst_ind + 0] = base_col * dist_val;
      params.dst[dst_ind + 1] = center_col * dist_val;
      params.dst[dst_ind + 2] = center_base;
      params.dst[dst_ind + 3] = shape_mask_val;
    }
  }
}

void image::apply_petal_texture_material(const float* src, const Descriptor& src_desc,
                                         const Vec3f& color0, const Vec3f& color1,
                                         const Vec3f& color2, const Vec3f& color3,
                                         bool to_srgb, float* dst) {
  assert(src_desc.num_channels() == 4);
  for (int i = 0; i < src_desc.rows(); i++) {
    for (int j = 0; j < src_desc.cols(); j++) {
      const int ind = ij_to_linear(i, j, src_desc);
      auto col0 = lerp(src[ind + 0], color0, color1);
      auto col1 = lerp(src[ind + 1], color2, color3);
      auto col = lerp(src[ind + 2], col0, col1);
      if (to_srgb) {
        col = linear_to_srgb(col);
      }
      for (int c = 0; c < 3; c++) {
        dst[ind + c] = col[c];
      }
      dst[ind + 3] = src[ind + 3];
    }
  }
}

GROVE_NAMESPACE_END