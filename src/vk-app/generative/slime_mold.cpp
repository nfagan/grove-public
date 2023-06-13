#include "slime_mold.hpp"
#include "grove/math/random.hpp"
#include "grove/math/constants.hpp"
#include "grove/math/util.hpp"
#include "grove/visual/image_process.hpp"
#include "grove/common/platform.hpp"
#include "grove/common/profile.hpp"
#include "grove/common/common.hpp"
#include "grove/common/util.hpp"

#ifdef GROVE_MACOS
#define GROVE_USE_PAR_EXEC (0)
#else
#define GROVE_USE_PAR_EXEC (1)
#endif

#if GROVE_USE_PAR_EXEC
#include <execution>
#endif

GROVE_NAMESPACE_BEGIN

namespace {

using namespace gen;
using Config = SlimeMoldConfig;

constexpr int data_texture_size() {
  constexpr int tex_dim = Config::texture_dim;
  constexpr int num_channels = Config::num_texture_channels;
  return tex_dim * tex_dim * num_channels;
}

Vec3f channel_weights(float center_scale, float rand_scale, float gain) {
  Vec3f center{};
  auto ind = int(urand() * 3.0);
  center[ind] = center_scale;
  auto c = normalize(center + Vec3f{urandf(), urandf(), urandf()} * rand_scale);
  c = clamp_each(c * gain, Vec3f{}, Vec3f{1.0f});
  return c;
}

Vec3f default_channel_weights() {
  return channel_weights(1.0f, 0.05f, 1.0f);
}

SlimeParticle make_particle(const Config& config, const Vec2f& pos, float heading) {
  SlimeParticle result{};
  result.position = pos;
  result.heading = heading;
  result.left_sensor = grove::pif() * (0.25f + urand_11f() * 0.1f);
  result.right_sensor = -grove::pif() * (0.25f + urand_11f() * 0.1f);
  result.sensor_step_size = 0.02f;
  result.sensor_size = 0.01f;
  result.speed = 0.1f;
  result.deposit = 1.0f;
  //  result.channel_weights = channel_weight(1.0f, 0.5f, 1.25f);
  result.channel_weights = default_channel_weights();
  result.sensor_speed_sensitivity = 1.0f + urand_11f() * 0.2f;
  result.sensor_speed_sensitivity_scale = 0.1f;
  result.turn_speed = pif() * 1.0f + urand_11f() * 0.5f;
  result.right_only = config.only_right_turns;

  result.turn_speed *= std::pow(2.0f, float(config.turn_speed_power));
  result.speed *= std::pow(2.0f, float(config.scale_speed_power));

  return result;
}

std::unique_ptr<float[]> make_texture_data() {
  return std::make_unique<float[]>(data_texture_size());
}

void set_random_data(float* out, int r, int c, int nc) {
  for (int i = 0; i < r * c * nc; i++) {
    out[i] = urandf();
  }
}

Vec2<int> to_ij(const Vec2f& p, int r, int c) {
  return {int(std::floor(p.x * float(c))), int(std::floor(p.y * float(r)))};
}

int data_offset(int i, int j, int rows, int channels) {
  return (j * rows + i) * channels;
}

template <typename Op>
void apply_in_circle(float* im, int r, int c, int nc,
                     const Vec2f& p, float radius, const float* value, Op&& op) {
  auto [imid, jmid] = to_ij(p, r, c);
  auto [i0, j0] = to_ij(p - radius, r, c);
  auto [i1, j1] = to_ij(p + radius, r, c);
  const auto r_px = std::max(1.0f, radius * float(std::max(r, c)));
  const auto r2 = r_px * r_px;

  for (int i = i0; i <= i1; i++) {
    for (int j = j0; j <= j1; j++) {
      Vec2f span{float(i - imid), float(j - jmid)};
      if (span.length_squared() <= r2 && i >= 0 && j >= 0 && i < r && j < c) {
        auto off = data_offset(i, j, r, nc);
        for (int k = 0; k < nc; k++) {
          im[off + k] = op(im[off + k], value[k]);
        }
      }
    }
  }
}

void clamped_add(float* im, int r, int c, int nc,
                 const Vec2f& p, float radius, const float* value) {
  apply_in_circle(im, r, c, nc, p, radius, value, [](float a, float b) {
    return clamp(a + b, 0.0f, 1.0f);
  });
}

Vec3f sample3(const float* data, int i, int j, int rows, int channels) {
  Vec3f res{};
  auto* s0 = data + data_offset(i, j, rows, channels);
  for (int k = 0; k < 3; k++) {
    res[k] += s0[k];
  }
  return res;
}

void deposit(const SlimeParticle& part, float* data) {
  constexpr auto td = Config::texture_dim;
  constexpr auto nc = Config::num_texture_channels;
  const auto [i, j] = to_ij(part.position, td, td);
  auto* out = data + data_offset(i, j, td, nc);
  const auto num_copy = std::min(3, nc);
  for (int k = 0; k < num_copy; k++) {
    out[k] = std::min(1.0f, out[k] + part.deposit * part.channel_weights[k]);
  }
}

Vec3f sense(const float* data, const Vec2f& p, float win_size, bool average = false) {
  static_assert(Config::num_texture_channels == 3);
  Vec3f result{};

  auto p0 = p - win_size * 0.5f;
  auto p1 = p + win_size * 0.5f;

  auto [i0, j0] = to_ij(p0, Config::texture_dim, Config::texture_dim);
  auto [i1, j1] = to_ij(p1, Config::texture_dim, Config::texture_dim);
  int ct{};

  for (int i = i0; i <= i1; i++) {
    for (int j = j0; j <= j1; j++) {
      if (i >= 0 && j >= 0 && i < Config::texture_dim && j < Config::texture_dim) {
        result += sample3(data, i, j, Config::texture_dim, Config::num_texture_channels);
        ct++;
      }
    }
  }

  if (average && ct > 0) {
    result /= float(ct);
  }

  return result;
}

Vec3f sense_circular(const float* data, const Vec2f& p, float win_size, bool average) {
  static_assert(Config::num_texture_channels == 3);
  Vec3f result{};

  auto p0 = p - win_size * 0.5f;
  auto p1 = p + win_size * 0.5f;

  auto [i0, j0] = to_ij(p0, Config::texture_dim, Config::texture_dim);
  auto [i1, j1] = to_ij(p1, Config::texture_dim, Config::texture_dim);
  int ct{};

  for (int i = i0; i <= i1; i++) {
    for (int j = j0; j <= j1; j++) {
      int is = wrap_within_range(i, Config::texture_dim);
      int js = wrap_within_range(j, Config::texture_dim);
      result += sample3(data, is, js, Config::texture_dim, Config::num_texture_channels);
      ct++;
    }
  }

  if (average && ct > 0) {
    result /= float(ct);
  }

  return result;
}

Vec2f to_vec(float t) {
  return {std::cos(t), std::sin(t)};
}

float wrap01(float v) {
  while (v < 0.0f) {
    v += 1.0f;
  }
  while (v >= 1.0f) {
    v -= 1.0f;
  }
  return v;
}

Vec2f wrap01(Vec2f v) {
  v.x = wrap01(v.x);
  v.y = wrap01(v.y);
  return v;
}

void update_particle(const Config& config, SlimeParticle& part, const float* im) {
  auto head = to_vec(part.heading);
  auto left = to_vec(part.left_sensor);
  auto right = to_vec(part.right_sensor);

  auto vf = sense(im, part.position + head * part.sensor_step_size, part.sensor_size);
  auto vl = sense(im, part.position + left * part.sensor_step_size, part.sensor_size);
  auto vr = sense(im, part.position + right * part.sensor_step_size, part.sensor_size);

  vf *= part.channel_weights;
  vl *= part.channel_weights;
  vr *= part.channel_weights;

  float vs[3] = {vf.length(), vl.length(), vr.length()};
  auto i = int(std::max_element(vs, vs+3) - vs);

  auto new_head = part.heading;
  auto len = vs[i];
  const auto dt = config.dt();

  if (i != 0) {
    float left_sgn = part.right_only ? 0.0f : 1.0f;
    float sgn = i == 1 ? left_sgn : -1.0f;
    new_head += sgn * part.turn_speed * dt;
  }

  auto speed_sens = 1.0f - std::exp(-len * part.sensor_speed_sensitivity);
  auto speed = part.speed + part.sensor_speed_sensitivity_scale * speed_sens;

  auto new_pos = part.position + to_vec(new_head) * speed * dt;
  if (config.circular_world) {
    new_pos = wrap01(new_pos);
  } else if (new_pos.x < 0.0f || new_pos.y < 0.0f || new_pos.x >= 1.0f || new_pos.y >= 1.0f) {
    const float eps = 0.001f;
    new_pos = clamp_each(new_pos, Vec2f{eps}, Vec2f{1.0f-eps});
    new_head = urandf() * 2.0f * pif();
  }

  part.heading = new_head;
  part.position = new_pos;
}

float power_to_scale(int current, int desired) {
  float scale{1.0f};
  if (current < desired) {
    for (int i = current; i < desired; i++) {
      scale *= 2.0f;
    }
  } else {
    for (int i = desired; i < current; i++) {
      scale *= 0.5f;
    }
  }
  return scale;
}

template <int N>
void box_filter(const float* a, float* out, float* tmp, int r, int c, int k_size) {
  image::simple_box_filter<float, N>(a, out, tmp, r, c, k_size);
}

template <typename Op>
void component_wise(Op&& op, int r, int c, int nc) {
  for (int i = 0; i < r * c * nc; i++) {
    op(i);
  }
}

template <typename Op>
void component_wise(Op&& op) {
  const auto td = Config::texture_dim;
  const auto nc = Config::num_texture_channels;
  component_wise(op, td, td, nc);
}

void im_lerp(const float* a, const float* b, float* out, float t) {
  component_wise([t, a, b, out](int off) {
    out[off] = lerp(t, a[off], b[off]);
  });
}

void im_decay(const float* a, float* out, float decay) {
  component_wise([a, out, decay](int off) {
    out[off] = std::max(0.0f, a[off] - decay);
  });
}

void diffuse(float* a, float* b, float* tmp, float decay, float diff_speed, int filter_size) {
  constexpr auto nc = Config::num_texture_channels;
  box_filter<nc>(a, b, tmp, Config::texture_dim, Config::texture_dim, filter_size);
  im_lerp(a, b, a, diff_speed);
  im_decay(a, a, decay);
}

void set_perturb_data(const Config& config, const float* im, float* out) {
  constexpr auto dim = Config::texture_dim;
  constexpr auto nc = Config::num_texture_channels;
  static_assert(nc == 3);

  if (config.perturb_event_type == 1) {
    set_random_data(out, dim, dim, nc);

    auto tmp0 = make_texture_data();
    auto tmp1 = make_texture_data();
    box_filter<nc>(out, tmp0.get(), tmp1.get(), dim, dim, 5);
    std::copy(tmp0.get(), tmp0.get() + data_texture_size(), out);

    component_wise([out, im](int off) {
      out[off] = (1.0f - im[off]) * std::min(1.0f, std::pow(out[off], 8.0f) * 2.0f);
    });
  } else {
    std::fill(out, out + data_texture_size(), 0.0f);
    for (int i = 0; i < config.num_perturb_circles; i++) {
      Vec2f center{urandf(), urandf()};
      const auto r = 0.1f;
      auto add = Vec3f{urandf(), urandf(), urandf()} * 0.5f;
      add[int(urand() * 3.0)] = urandf() * 0.25f + 0.75f;
      float add_array[3] = {add.x, add.y, add.z};
      clamped_add(out, dim, dim, nc, center, r, add_array);
    }
  }
}

void apply_perturb(const float* perturb, float* out) {
  component_wise([perturb, out](int off) {
    out[off] = std::min(1.0f, out[off] + perturb[off]);
  });
}

void apply_signal(const float* signal, float* out) {
  component_wise([signal, out](int off) {
    out[off] = std::max(signal[off], out[off]);
  });
}

void set_signal_data(float* im, const gen::SlimeMoldParams& params) {
  constexpr auto dim = Config::texture_dim;
  constexpr auto nc = Config::num_texture_channels;
  static_assert(nc == 3);

  std::fill(im, im + data_texture_size(), 0.0f);
  auto add = params.channel_mask * params.signal_value;
  float add_array[3] = {add.x, add.y, add.z};
  clamped_add(im, dim, dim, nc, params.signal_position, params.signal_radius, add_array);
}

void scale_turn_speed(SlimeParticle* parts, int num_parts, float scale) {
  for (int i = 0; i < num_parts; i++) {
    parts[i].turn_speed *= scale;
  }
}

void scale_speed(SlimeParticle* parts, int num_parts, float scale) {
  for (int i = 0; i < num_parts; i++) {
    parts[i].speed *= scale;
  }
}

#if 0
void randomize_channel_weights(SlimeParticle* parts, int num_parts) {
  for (int i = 0; i < num_parts; i++) {
    parts[i].channel_weights = default_channel_weights();
  }
}
#endif

void set_right_only(SlimeParticle* parts, int num_parts, bool v) {
  for (int i = 0; i < num_parts; i++) {
    parts[i].right_only = v;
  }
}

} //  anon

std::unique_ptr<float[]> gen::make_slime_mold_texture_data() {
  return make_texture_data();
}

DefaultSlimeMoldSimulationTextureData gen::make_default_slime_mold_texture_data() {
  DefaultSlimeMoldSimulationTextureData result;
  result.texture_data0 = make_slime_mold_texture_data();
  result.texture_data1 = make_slime_mold_texture_data();
  result.texture_data2 = make_slime_mold_texture_data();
  result.perturb_data = make_slime_mold_texture_data();
  result.signal_data = make_slime_mold_texture_data();
  return result;
}

std::unique_ptr<SlimeParticle[]> gen::make_slime_mold_particles(const SlimeMoldConfig& config) {
  auto result = std::make_unique<SlimeParticle[]>(config.num_particles);
  for (int i = 0; i < config.num_particles; i++) {
    auto pos = Vec2f{urand_11f(), urand_11f()} * Config::starting_offset_span + 0.5f;
    auto head = urandf() * 2.0f * pif();
    result[i] = make_particle(config, pos, head);
  }
  return result;
}

void gen::update_slime_mold_particles(SlimeParticle* particles,
                                      const SlimeMoldConfig& config,
                                      SlimeMoldSimulationContext* context) {
  GROVE_PROFILE_TIC("gen/slime_mold/update");
  auto* data0 = context->texture_data0;
  auto* data1 = context->texture_data1;
  auto* tmp = context->texture_data2;

#if GROVE_USE_PAR_EXEC
  std::for_each(std::execution::par, particles, particles + config.num_particles,
                [data0, &config](auto&& part) {
    update_particle(config, part, data0);
  });
#else
  for (int i = 0; i < config.num_particles; i++) {
    update_particle(config, particles[i], data0);
  }
#endif
  for (int i = 0; i < config.num_particles; i++) {
    deposit(particles[i], data0);
  }

  if (config.diffuse_enabled) {
    diffuse(data0, data1, tmp, config.decay, config.diffuse_speed, config.filter_size);
  }

  if (!context->set_perturb_data) {
    set_perturb_data(config, data0, context->perturb_data);
    context->set_perturb_data = true;
  }

  context->tot_iter++;
  if (config.allow_perturb_event && (context->tot_iter % config.perturb_interval == 0)) {
    set_perturb_data(config, data0, context->perturb_data);
    context->perturb_state = 1;
  }

  if (config.allow_signal_influence) {
    set_signal_data(context->signal_data, *context->params);
    apply_signal(context->signal_data, data0);
  }

  switch (context->perturb_state) {
    case 1: {
      apply_perturb(context->perturb_data, data0);
      if (context->perturb_iters++ >= config.num_perturb_iters) {
        context->perturb_iters = 0;
        context->perturb_state = 0;
      }
      break;
    }
  }
  GROVE_PROFILE_TOC("gen/slime_mold/update");
}

void gen::set_particle_turn_speed_power(SlimeParticle* particles,
                                        SlimeMoldConfig& config,
                                        int new_power) {
  float scale = power_to_scale(config.turn_speed_power, new_power);
  if (scale != 1.0f) {
    scale_turn_speed(particles, config.num_particles, scale);
    config.turn_speed_power = new_power;
  }
}

void gen::set_particle_speed_power(SlimeParticle* particles,
                                   SlimeMoldConfig& config,
                                   int new_power) {
  float scale = power_to_scale(config.scale_speed_power, new_power);
  if (scale != 1.0f) {
    scale_speed(particles, config.num_particles, scale);
    config.scale_speed_power = new_power;
  }
}

void gen::set_particle_right_only(SlimeParticle* particles, SlimeMoldConfig& config, bool value) {
  set_right_only(particles, config.num_particles, value);
}

Vec3f gen::sample_slime_mold_texture_data01(const float* data, const Vec2f& p01, float r01) {
  const bool average = true;
  auto v = sense_circular(data, p01, r01, average);
  return clamp_each(v, Vec3f{}, Vec3f{1.0f});
}

void gen::add_value(float* data, const Vec2f& p01, float radius01, const Vec3f& v3) {
  const auto nc = Config::num_texture_channels;
  const auto td = Config::texture_dim;
  const float v[3]{v3.x, v3.y, v3.z};
  clamped_add(data, td, td, nc, p01, radius01, v);
}

void gen::add_value(float* data,
                    int tex_dim,
                    int tex_components,
                    const Vec2f& p01,
                    float radius01,
                    const Vec3f& v3) {
  const float v[3]{v3.x, v3.y, v3.z};
  clamped_add(data, tex_dim, tex_dim, tex_components, p01, radius01, v);
}

GROVE_NAMESPACE_END
