#pragma once

#include "ray_project.hpp"

namespace grove::debug {

struct RenderProjectRayParams {
  Vec3f scale{1.0f};
  Vec3f offset{};
  Vec3f ray_color{1.0f, 0.0f, 0.0f};
  const Vec3f* ns{nullptr};
  float offset_normal_length{};
};

void render_project_ray_results(const ProjectRayResultEntry* entries,
                                uint32_t num_entries,
                                const uint32_t* tris,
                                const Vec3f* ps,
                                const RenderProjectRayParams& params);

}