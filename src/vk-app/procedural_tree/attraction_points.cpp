#include "attraction_points.hpp"
#include "grove/common/common.hpp"
#include "grove/math/random.hpp"

GROVE_NAMESPACE_BEGIN

Vec3f points::uniform_sphere() {
  auto p = Vec3f{urand_11f(), urand_11f(), urand_11f()};
  while (p.length() > 1.0f) {
    p = Vec3f{urand_11f(), urand_11f(), urand_11f()};
  }
  return p;
}

Vec3f points::uniform_hemisphere() {
  auto p = Vec3f{urand_11f(), urandf(), urand_11f()};
  while (p.length() > 1.0f) {
    p = Vec3f{urand_11f(), urandf(), urand_11f()};
  }
  return p;
}

std::vector<Vec3f> points::uniform_sphere(int count, const Vec3f& scale, const Vec3f& off) {
  std::vector<Vec3f> result(count);
  for (auto& p : result) {
    p = points::uniform_sphere() * scale + off;
  }
  return result;
}

void points::uniform_sphere(Vec3f* dst, int count, const Vec3f& scale, const Vec3f& off) {
  for (int i = 0; i < count; i++) {
    dst[i] = points::uniform_sphere() * scale + off;
  }
}

std::vector<Vec3f> points::uniform_hemisphere(int count, const Vec3f& scale, const Vec3f& off) {
  std::vector<Vec3f> result(count);
  for (auto& p : result) {
    p = points::uniform_hemisphere() * scale + off;
  }
  return result;
}

void points::uniform_hemisphere(Vec3f* dst, int count, const Vec3f& scale, const Vec3f& off) {
  for (int i = 0; i < count; i++) {
    dst[i] = uniform_hemisphere() * scale + off;
  }
}

void points::uniform_cylinder_to_hemisphere(Vec3f* dst, int count,
                                            const Vec3f& scale, const Vec3f& off) {
  uniform_sphere(dst, count);
  for (int i = 0; i < count; i++) {
    auto& p = dst[i];
    if (p.y < 0.0f) {
      auto neg_factor = std::pow(1.0f - std::abs(p.y), 4.0f);
      p.x *= neg_factor;
      p.z *= neg_factor;
    } else {
      p.y *= 0.5f;
    }
    p.y = p.y * 0.5f + 0.5f;
    p = p * scale + off;
  }
}

std::vector<Vec3f> points::uniform_cylinder_to_hemisphere(int count,
                                                          const Vec3f& scale,
                                                          const Vec3f& off) {
#if 0
  auto ps = uniform_sphere(count);
  for (auto& p : ps) {
    if (p.y < 0.0f) {
      auto neg_factor = std::pow(1.0f - std::abs(p.y), 4.0f);
      p.x *= neg_factor;
      p.z *= neg_factor;
    } else {
      p.y *= 0.5f;
    }

    p.y = p.y * 0.5f + 0.5f;
    p = p * scale + off;
  }
  return ps;
#else
  std::vector<Vec3f> result(count);
  uniform_cylinder_to_hemisphere(result.data(), count, scale, off);
  return result;
#endif
}

GROVE_NAMESPACE_END
