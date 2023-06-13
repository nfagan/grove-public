#pragma once

#include "grove/math/vector.hpp"
#include <vector>

namespace grove::points {

std::vector<Vec3f> uniform_sphere(int count,
                                  const Vec3f& scale = Vec3f{1.0f},
                                  const Vec3f& off = Vec3f{});
void uniform_sphere(Vec3f* dst, int count,
                    const Vec3f& scale = Vec3f{1.0f}, const Vec3f& off = {});

std::vector<Vec3f> uniform_hemisphere(int count,
                                      const Vec3f& scale = Vec3f{1.0f},
                                      const Vec3f& off = Vec3f{});
void uniform_hemisphere(Vec3f* dst, int count,
                        const Vec3f& scale = Vec3f{1.0f}, const Vec3f& off = {});

std::vector<Vec3f> uniform_cylinder_to_hemisphere(int count,
                                                  const Vec3f& scale = Vec3f{1.0f},
                                                  const Vec3f& off = Vec3f{});
void uniform_cylinder_to_hemisphere(Vec3f* dst, int count, const Vec3f& scale, const Vec3f& off);

Vec3f uniform_hemisphere();
Vec3f uniform_sphere();

}