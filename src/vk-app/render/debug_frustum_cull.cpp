#include "debug_frustum_cull.hpp"
#include "frustum_cull_data.hpp"
#include "grove/common/common.hpp"
#include "debug_draw.hpp"

GROVE_NAMESPACE_BEGIN

void cull::debug::draw_frustum_cull_data(const FrustumCullData* sys, const Vec3f& color) {
  for (const auto* group = sys->group_alloc.read_group_begin();
       group != sys->group_alloc.read_group_end();
       ++group) {
    assert(group->offset + group->count <= uint32_t(sys->instances.size()));
    for (uint32_t i = 0; i < group->count; i++) {
      const auto& aabb4 = sys->instances[i + group->offset];
      Bounds3f bounds{to_vec3(aabb4.aabb_p0), to_vec3(aabb4.aabb_p1)};
      vk::debug::draw_aabb3(bounds, color);
    }
  }
}

GROVE_NAMESPACE_END
