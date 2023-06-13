#include "UIPlaneCloth.hpp"
#include "grove/math/matrix_transform.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

#define BOTTOM_CONSTRAINT (1)
#define CORNER_CONSTRAINTS (1)
#define CONSTRAIN_Y_TO_ZERO (0)
#define UP_ORIENT (1)

UIPlaneCloth::UIPlaneCloth() :
  positions(new Vec4f[num_particles]{}),
  velocities(new Vec3f[num_particles]{}),
  normals(new Vec3f[num_particles]{}),
  spring_forces(new Vec3f[num_particles]{}),
  external_forces(new Vec3f[num_particles]{}),
  wind_direction(normalize(Vec3f(0.25f, 0.0f, 1.0f))) {
  //
  initialize_particles();
}

void UIPlaneCloth::initialize_particles() {
  int index = 0;
  Vec3f center(particle_dim * rest_distance * 0.5f);
  center.z = 0.0;
#if !UP_ORIENT
  Mat4f rot = make_rotation(float(grove::pi_over_two()), Vec3f(1.0f, 0.0f, 0.0f));
#endif

  for (int i = 0; i < particle_dim; i++) {
    for (int j = 0; j < particle_dim; j++) {
      float x = float(i) * rest_distance;
      float y = float(j) * rest_distance;
#if UP_ORIENT
      positions[index] = Vec4f{x - center.x, y + rest_height, 0.0f, y + rest_height};
      normals[index] = Vec3f(0.0f, 0.0f, 1.0f);
#else
      Vec3f pos(x, y, 0.0f);
      pos -= center;

      auto pos4 = rot * Vec4f(pos, 1.0f);
      pos = Vec3f(pos4.x, pos4.y, pos4.z);
      pos.y += rest_height;

      positions[index] = Vec4f{pos, 0.0f};
      velocities[index] = Vec3f(0.0f);
      normals[index] = Vec3f(0.0f, 1.0f, 0.0f);
      spring_forces[index] = Vec3f(0.0f);
      external_forces[index] = Vec3f(0.0f);
#endif
      index++;
    }
  }
}

UIPlaneCloth::PositionData UIPlaneCloth::get_position_data(float height) const {
  const auto& pmin = positions[0];
  const auto& pmax = positions[particle_dim * particle_dim - 1];

  Vec3f bounds_p0{to_vec3(pmin)};
  Vec3f bounds_p1{to_vec3(pmax)};
#if UP_ORIENT
  (void) height;
  auto cent = bounds_p0 + (bounds_p1 - bounds_p0) * 0.5f;
  auto plane = Vec4f(0.0f, 0.0f, 1.0f, -cent.z);
#else
  auto plane = Vec4f(0.0f, 1.0f, 0.0f, -(height + rest_height));
#endif
  return {positions.get(), num_particles, bounds_p0, bounds_p1, plane};
}

void UIPlaneCloth::on_new_height_map(float height) {
  for (int i = 0; i < particle_dim * particle_dim; i++) {
#if UP_ORIENT
    positions[i].y = positions[i].w + height;
#else
    positions[i].y = height + rest_height;
#endif
  }
}

namespace {

inline Vec3f spring_force(const Vec3f& v, float rest_distance, float k) {
  return k * (v.length() - rest_distance) * normalize(v);
}

} //  anon

void UIPlaneCloth::set_spectral_multiplier(float value) {
  spectral_mean_multiplier = value;
}

void UIPlaneCloth::move(const Vec3f& vel) {
  for (int i = 0; i < particle_dim * particle_dim; i++) {
    positions[i] += Vec4f{vel, 0.0f};
  }
}

void UIPlaneCloth::calculate_normals() {
  for (int i = 1; i < particle_dim-1; i++) {
    for (int j = 1; j < particle_dim-1; j++) {
      int left = (i - 1) * particle_dim + j;
      int right = (i + 1) * particle_dim + j;
      int top = i * particle_dim + j + 1;
      int bot = i * particle_dim + j - 1;

      const float nx = positions[left].z - positions[right].z;
      const float ny = positions[bot].z - positions[top].z;
      const float nz = 2.0f * rest_distance;

      normals[i * particle_dim + j] = normalize(Vec3f(nx, ny, nz));
    }
  }
}

void UIPlaneCloth::set_external_forces(float spectral_mean) {
  int index = 0;
  const Vec3f g(0.0f, -9.8f, 0.0f);

  for (int i = 0; i < particle_dim; i++) {
    for (int j = 0; j < particle_dim; j++) {
      bool base_condition = j == particle_dim - 1;
#if BOTTOM_CONSTRAINT
      base_condition = base_condition || j == 0;
#endif
      if (base_condition) {
        external_forces[index] = Vec3f(0.0f);

      } else {
//        const auto wind_dir2 = wind.get_current_velocity(index);
        const auto wind_dir2 = Vec2f{0.75f};
        auto wind_dir = normalize(Vec3f(wind_dir2.x, 0.0f, wind_dir2.y));
        const auto spect_mag = spectral_mean;
        wind_dir += spect_mag * spectral_mean_multiplier;

        auto fg = particle_mass * g;
        auto fw = std::abs(dot(wind_dir - velocities[index], normals[index])) * k_wind;

        external_forces[index] = fg + fw;
      }

      index++;
    }
  }
}

Vec3f UIPlaneCloth::calculate_spring_force(float rest_dist,
                                           float k_spring,
                                           float k_damp,
                                           int ind_q,
                                           const Vec4f& p,
                                           const Vec3f& v) const {
  auto f_spring = spring_force(to_vec3(positions[ind_q] - p), rest_dist, k_spring);
  auto f_damp = k_damp * (velocities[ind_q] - v);
  return f_spring + f_damp;
}

void UIPlaneCloth::update(float spectral_mean) {
  //  const auto delta = timer.delta();
  //  const float dt = delta.count();
  const float dt = 1.0f / 60.0f;

  const float dt2 = dt * dt;
  const float rest_dist2 = rest_distance * 2.0f;
  const float root2_dist = std::sqrt(2.0f) * rest_distance;

#if !UP_ORIENT
  set_external_forces(spectral_mean);
#else
  (void) spectral_mean;
#endif

  for (int i = 0; i < particle_dim; i++) {
    for (int j = 0; j < particle_dim; j++) {
      Vec3f f_top(0.0f);
      Vec3f f_right(0.0f);
      Vec3f f_top_top(0.0f);
      Vec3f f_right_right(0.0f);

      int self = i * particle_dim + j;

      const auto p = positions[self];
      const auto v = velocities[self];

      if (j + 1 < particle_dim) {
        const int top = self + 1;
        f_top = calculate_spring_force(rest_distance, k_spring_adjacent, k_spring_far, top, p, v);
        spring_forces[top] -= f_top;
      }

      if (i + 1 < particle_dim) {
        const int right = (i + 1) * particle_dim + j;
        f_right = calculate_spring_force(
          rest_distance, k_spring_adjacent, k_damp_adjacent, right, p, v);
        spring_forces[right] -= f_right;
      }

      if (j + 2 < particle_dim) {
        const int top_top = self + 2;
        f_top_top = calculate_spring_force(
          rest_dist2, k_spring_far, k_damp_far, top_top, p, v);
        spring_forces[top_top] -= f_top_top;
      }

      if (i + 2 < particle_dim) {
        const int right_right = (i + 2) * particle_dim + j;
        f_right_right = calculate_spring_force(
          rest_dist2, k_spring_far, k_damp_far, right_right, p, v);
        spring_forces[right_right] -= f_right_right;
      }

      auto tot_force = f_top + f_right + f_top_top + f_right_right;

#if CORNER_CONSTRAINTS
      int corner_index = 0;

      for (int k = 0; k < 2; k++) {
        for (int h = 0; h < 2; h++) {
          const int i_incr = k == 0 ? -1 : 1;
          const int j_incr = h == 0 ? -1 : 1;
          const int ind_i = i + i_incr;
          const int ind_j = j + j_incr;

          if (ind_i >= 0 && ind_j >= 0 &&
          ind_i < particle_dim && ind_j < particle_dim) {
            const int ind = ind_i * particle_dim + ind_j;
            auto tmp_force =
              calculate_spring_force(root2_dist, k_spring_corner, k_damp_corner, ind, p, v);
            tot_force += tmp_force;
            spring_forces[ind] -= tmp_force;
          }

          corner_index++;
        }
      }
#endif

      spring_forces[self] += tot_force;
    }
  }

  //  Constraint on top.
  for (int i = 0; i < particle_dim; i++) {
    spring_forces[i * particle_dim + particle_dim - 1] = Vec3f(0.0f);
#if BOTTOM_CONSTRAINT
    spring_forces[i * particle_dim] = Vec3f(0.0f);
#endif
  }

  const float mass2 = particle_mass * 2.0f;

  for (int i = 0; i < num_particles; i++) {
    auto p = positions[i];
    auto p3 = to_vec3(p);
    auto v = velocities[i];
    auto f = spring_forces[i] + external_forces[i];
    auto new_pos = p3 + v * dt + (f / mass2) * dt2;

#if CONSTRAIN_Y_TO_ZERO
    if (new_pos.y < 0.0f) {
      auto& fe = spring_forces[i];
      auto& fs = external_forces[i];
      fs.y = mass2 * (-p.y - v.y * dt) / dt2 - fe.y;
      auto new_f = fs + fe;
      new_pos = p + v * dt + (new_f / mass2) * dt2;
    }
#endif
    velocities[i] = new_pos - p3;
#if UP_ORIENT
    positions[i] = Vec4f{new_pos, p.w};
#else
    positions[i] = Vec4f{new_pos, 0.0f};
#endif
  }

  calculate_normals();
}

GROVE_NAMESPACE_END
