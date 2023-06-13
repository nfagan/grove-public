#include "debug.hpp"
#include "../render/debug_draw.hpp"
#include "grove/math/triangle.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

void debug::render_project_ray_results(const ProjectRayResultEntry* entries,
                                       uint32_t num_entries,
                                       const uint32_t* tris,
                                       const Vec3f* ps,
                                       const RenderProjectRayParams& params) {
  const auto& scl = params.scale;
  const auto& off = params.offset;
  for (uint32_t ei = 0; ei < num_entries; ei++) {
    auto& entry = entries[ei];
    auto pi0 = tris[entry.ti * 3];
    auto pi1 = tris[entry.ti * 3 + 1];
    auto pi2 = tris[entry.ti * 3 + 2];
    auto tp0 = ps[pi0] * scl + off;
    auto tp1 = ps[pi1] * scl + off;
    auto tp2 = ps[pi2] * scl + off;

    auto p0 = to_vec3f(entry.entry_p) * scl + off;
    auto p1 = to_vec3f(entry.exit_p) * scl + off;
    if (params.offset_normal_length > 0.0f) {
      Vec3f n_curr;
      if (params.ns) {
        n_curr = normalize((params.ns[pi0] + params.ns[pi1] + params.ns[pi2]) / scl);
      } else {
        n_curr = tri::compute_normal(tp0, tp1, tp2);
      }

      Vec3f n_next = n_curr;
      if (ei + 1 < num_entries) {
        auto& next_entry = entries[ei+1];
        auto next_pi0 = tris[next_entry.ti * 3];
        auto next_pi1 = tris[next_entry.ti * 3 + 1];
        auto next_pi2 = tris[next_entry.ti * 3 + 2];
        if (params.ns) {
          n_next = normalize((
            params.ns[next_pi0] + params.ns[next_pi1] + params.ns[next_pi2]) / scl);
        } else {
          n_next = tri::compute_normal(
            ps[next_pi0] * scl + off,
            ps[next_pi1] * scl + off,
            ps[next_pi2] * scl + off);
        }
      }
      p0 += n_curr * params.offset_normal_length;
      p1 += n_next * params.offset_normal_length;
    }

    vk::debug::draw_line(p0, p1, params.ray_color);
    auto tri_color = entry.required_flip ? Vec3f{0.0f, 1.0f, 1.0f} : Vec3f{0.0f, 1.0f, 0.0f};
    vk::debug::draw_triangle_edges(tp0, tp1, tp2, tri_color);
  }
}

GROVE_NAMESPACE_END
