#include "common.hpp"
#include "grove/math/matrix_transform.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

void mat2_to_ijk(const Mat2f& m, Vec3f* i, Vec3f* j, Vec3f* k) {
  *i = {m(0, 0), 0.0f, m(1, 0)};
  *j = {0.0f, 1.0f, 0.0f};
  *k = {m(0, 1), 0.0f, m(1, 1)};
}

} //  anon

OBB3f arch::make_obb_xz(const Vec3f& c, float theta, const Vec3f& full_size) {
  auto m = make_rotation(theta);
  Vec3f i;
  Vec3f j;
  Vec3f k;
  mat2_to_ijk(m, &i, &j, &k);
  Vec3f p{c.x, full_size.y * 0.5f + c.y, c.z};
  Vec3f s{full_size * 0.5f};
  return OBB3f{i, j, k, p, s};
}

OBB3f arch::extrude_obb_xz(const OBB3f& a, float dth, const Vec3f& full_size) {
  Vec2f i_xz{a.i.x, a.i.z};
  Vec2f k_xz{a.k.x, a.k.z};
  Mat2f a_xz{i_xz, k_xz};
  Mat2f a2_xz = make_rotation(dth) * a_xz;
  Vec3f i2;
  Vec3f j2;
  Vec3f k2;
  mat2_to_ijk(a2_xz, &i2, &j2, &k2);
  auto s2 = full_size * 0.5f;

  float z_sign = dth <= 0.0f ? -1.0f : 1.0f;
  Vec2f p1_xz{a.position.x, a.position.z};
  p1_xz += a_xz * Vec2f{a.half_size.x, z_sign * a.half_size.z};

  auto cent_xz = p1_xz + Vec2f{i2.x, i2.z} * s2.x;
  auto z_incr = Vec2f{k2.x, k2.z} * s2.z;
  if (dth <= 0.0f) {
    cent_xz += z_incr;
  } else {
    cent_xz -= z_incr;
  }

  auto p2 = Vec3f{
    cent_xz.x,
    a.position.y + (s2.y - a.half_size.y),
    cent_xz.y
  };

  return OBB3f{i2, j2, k2, p2, s2};
}

GROVE_NAMESPACE_END
