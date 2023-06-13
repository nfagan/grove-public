#include "debug_draw.hpp"
#include "../vk/vk.hpp"
#include "PointBufferRenderer.hpp"
#include "SimpleShapePools.hpp"
#include "grove/common/common.hpp"
#include "grove/math/Mat4.hpp"
#include "grove/math/frame.hpp"
#include <vector>

GROVE_NAMESPACE_BEGIN

namespace {

using namespace grove::vk;

constexpr uint32_t line_pool_reserve_size() {
  return 256;
}

constexpr int cube_pool_reserve_size() {
  return 128;
}

constexpr int plane_pool_reserve_size() {
  return 128;
}

struct LinePool {
  struct Entry {
    PointBufferRenderer::DrawableHandle handle;
    uint32_t num_reserved;
    uint32_t num_active;
    bool is_active;
  };

  std::vector<Entry> entries;
};

struct DebugRenderContext {
  PointBufferRenderer* point_buffer_renderer;
  SimpleShapeRenderer* simple_shape_renderer;
  const Core* core;
  Allocator* allocator;
  BufferSystem* buffer_system;
  StagingBufferSystem* staging_buffer_system;
  CommandProcessor* command_processor;
  uint32_t frame_queue_depth;
  LinePool line_pool;
  SimpleShapePools cube_pools;
  SimpleShapePools plane_pools;
  bool initialized;
} render_context{};

PointBufferRenderer::AddResourceContext
to_pb_renderer_add_resource_context(const DebugRenderContext& ctx) {
  return {
    *ctx.core,
    ctx.allocator,
    *ctx.buffer_system,
    ctx.frame_queue_depth
  };
}

SimpleShapeRenderer::AddResourceContext
to_shape_renderer_add_resource_context(const DebugRenderContext& ctx) {
  return {
    *ctx.core,
    ctx.allocator,
    *ctx.command_processor,
    *ctx.buffer_system,
    *ctx.staging_buffer_system,
    ctx.frame_queue_depth
  };
}

void draw_box_lines(const Vec3f* vs, const Vec3f& color) {
  //  face0
  debug::draw_line(vs[0], vs[1], color);
  debug::draw_line(vs[1], vs[2], color);
  debug::draw_line(vs[2], vs[3], color);
  debug::draw_line(vs[3], vs[0], color);
  //  face1
  debug::draw_line(vs[4], vs[5], color);
  debug::draw_line(vs[5], vs[6], color);
  debug::draw_line(vs[6], vs[7], color);
  debug::draw_line(vs[7], vs[4], color);
  //  length-wise
  debug::draw_line(vs[0], vs[4], color);
  debug::draw_line(vs[1], vs[5], color);
  debug::draw_line(vs[2], vs[6], color);
  debug::draw_line(vs[3], vs[7], color);
}

} //  anon

void vk::debug::initialize_rendering(PointBufferRenderer* pb_renderer,
                                     SimpleShapeRenderer* simple_shape_renderer,
                                     const Core* core,
                                     Allocator* allocator,
                                     BufferSystem* buffer_system,
                                     StagingBufferSystem* staging_buffer_system,
                                     CommandProcessor* command_processor,
                                     uint32_t frame_queue_depth) {
  render_context.point_buffer_renderer = pb_renderer;
  render_context.simple_shape_renderer = simple_shape_renderer;
  render_context.core = core;
  render_context.allocator = allocator;
  render_context.buffer_system = buffer_system;
  render_context.staging_buffer_system = staging_buffer_system;
  render_context.command_processor = command_processor;
  render_context.frame_queue_depth = frame_queue_depth;

  auto res_ctx = to_shape_renderer_add_resource_context(render_context);
  if (auto cube = simple_shape_renderer->require_cube(res_ctx)) {
    render_context.cube_pools = SimpleShapePools{
      cube.value(),
      cube_pool_reserve_size(),
      SimpleShapePools::ReleaseEnabled::No,
      SimpleShapeRenderer::PipelineType::NonOriented
    };
  }

  if (auto plane = simple_shape_renderer->require_plane(res_ctx)) {
    render_context.plane_pools = SimpleShapePools{
      plane.value(),
      plane_pool_reserve_size(),
      SimpleShapePools::ReleaseEnabled::No,
      SimpleShapeRenderer::PipelineType::Oriented
    };
  }

  render_context.initialized = true;
}

void vk::debug::reset_rendering() {
  for (auto& entry : render_context.line_pool.entries) {
    entry.num_active = 0;
    if (entry.is_active) {
      render_context.point_buffer_renderer->clear_active_instances(entry.handle);
      render_context.point_buffer_renderer->remove_active_drawable(entry.handle);
      entry.is_active = false;
    }
  }
  if (render_context.cube_pools.is_valid()) {
    render_context.cube_pools.reset(*render_context.simple_shape_renderer);
  }
  if (render_context.plane_pools.is_valid()) {
    render_context.plane_pools.reset(*render_context.simple_shape_renderer);
  }
}

void vk::debug::draw_triangle_edges(const Vec3f& p0, const Vec3f& p1, const Vec3f& p2,
                                    const Vec3f& color) {
  draw_line(p0, p1, color);
  draw_line(p1, p2, color);
  draw_line(p2, p0, color);
}

void vk::debug::draw_triangle_edges(const uint32_t* ti, uint32_t num_triangles,
                                    const Vec3f* ps, const Vec3f& color,
                                    const Vec3f& scale, const Vec3f& offset) {
  for (uint32_t i = 0; i < num_triangles; i++) {
    auto& p0 = ps[ti[i * 3]];
    auto& p1 = ps[ti[i * 3 + 1]];
    auto& p2 = ps[ti[i * 3 + 2]];
    draw_triangle_edges(p0 * scale + offset, p1 * scale + offset, p2 * scale + offset, color);
  }
}

void vk::debug::draw_lines(const Vec3f* p, int num_points, const Vec3f& color) {
  //  @TODO: Optimize this path.
  int num_lines = num_points / 2;
  for (int i = 0; i < num_lines; i++) {
    auto p0 = p[i * 2];
    auto p1 = p[i * 2 + 1];
    draw_line(p0, p1, color);
  }
}

void vk::debug::draw_connected_line(const Vec3f* p, int num_points, const Vec3f& color,
                                    bool wrap_around) {
  for (int i = 0; i < num_points-1; i++) {
    draw_line(p[i], p[i + 1], color);
  }
  if (wrap_around && num_points > 2) {
    draw_line(p[0], p[num_points - 1], color);
  }
}

void vk::debug::draw_line(const Vec3f& p0, const Vec3f& p1, const Vec3f& color) {
  if (!render_context.initialized) {
    return;
  }

  LinePool::Entry* pool_entry{};
  for (auto& entry : render_context.line_pool.entries) {
    if (entry.num_active + 1 < entry.num_reserved) {  //  +1 for p0 and p1
      pool_entry = &entry;
      break;
    }
  }

  const auto res_ctx = to_pb_renderer_add_resource_context(render_context);
  if (!pool_entry) {
    auto handle = render_context.point_buffer_renderer->create_drawable(
      PointBufferRenderer::DrawableType::Lines, {});
    render_context.point_buffer_renderer->reserve_instances(
      res_ctx,
      handle,
      line_pool_reserve_size());
    auto& entry = render_context.line_pool.entries.emplace_back();
    entry.num_reserved = line_pool_reserve_size();
    entry.num_active = 0;
    entry.handle = handle;
    entry.is_active = false;
    pool_entry = &entry;
  }

  if (!pool_entry->is_active) {
    render_context.point_buffer_renderer->add_active_drawable(pool_entry->handle);
    pool_entry->is_active = true;
  }

  const Vec3f points[2] = {p0, p1};
  const Vec3f colors[2] = {color, color};

  uint32_t inst_off = pool_entry->num_active;
  pool_entry->num_active += 2;
  render_context.point_buffer_renderer->set_instances(
    res_ctx,
    pool_entry->handle,
    points,
    2,
    int(inst_off));
  render_context.point_buffer_renderer->set_instance_color_range(
    res_ctx,
    pool_entry->handle,
    colors,
    2,
    int(inst_off));
}

void vk::debug::draw_obb3(const OBB3f& obb, const Vec3f& color) {
  Vec3f vs[8];
  gather_vertices(obb, vs);
  draw_box_lines(vs, color);
}

void vk::debug::draw_aabb3(const Bounds3f& aabb, const Vec3f& color) {
  Vec3f vs[8];
  gather_vertices(aabb, vs);
  draw_box_lines(vs, color);
}

void vk::debug::draw_cube(const Vec3f& p, const Vec3f& s, const Vec3f& color) {
  if (!render_context.initialized || !render_context.cube_pools.is_valid()) {
    return;
  }
  auto res_ctx = to_shape_renderer_add_resource_context(render_context);
  auto& pools = render_context.cube_pools;
  if (auto handle = pools.acquire(res_ctx, *render_context.simple_shape_renderer)) {
    //
    render_context.simple_shape_renderer->set_instance_params(
      handle.value().drawable_handle,
      handle.value().instance_index,
      color, s, p);
  }
}

void vk::debug::draw_plane(const Vec3f& p, const Vec3f& n, const Vec2f& s, const Vec3f& color) {
  Vec3f i;
  Vec3f j;
  Vec3f k;
  make_coordinate_system_y(n, &i, &j, &k);
  draw_plane(p, i, k, s, color);
}

void vk::debug::draw_plane(const Vec3f& p, const Vec3f& x, const Vec3f& y, const Vec2f& s,
                           const Vec3f& color) {
  if (!render_context.initialized || !render_context.plane_pools.is_valid()) {
    return;
  }
  auto res_ctx = to_shape_renderer_add_resource_context(render_context);
  auto& pools = render_context.plane_pools;
  if (auto handle = pools.acquire(res_ctx, *render_context.simple_shape_renderer)) {
    render_context.simple_shape_renderer->set_oriented_instance_params(
      handle.value().drawable_handle,
      handle.value().instance_index,
      color, Vec3f{s.x, s.y, 1.0f}, p, x, y);
  }
}

void vk::debug::draw_frustum_lines(float s, float g, float n, float f, const Mat4f& inv_view,
                                   const Vec3f& color) {
  const float xn = n * s / g;
  const float xf = f * s / g;
  const float yn = n / g;
  const float yf = f / g;

  auto p0n = to_vec3(inv_view * Vec4f{-xn, -yn, n, 1.0f});
  auto p1n = to_vec3(inv_view * Vec4f{xn, -yn, n, 1.0f});
  auto p2n = to_vec3(inv_view * Vec4f{xn, yn, n, 1.0f});
  auto p3n = to_vec3(inv_view * Vec4f{-xn, yn, n, 1.0f});

  auto p0f = to_vec3(inv_view * Vec4f{-xf, -yf, f, 1.0f});
  auto p1f = to_vec3(inv_view * Vec4f{xf, -yf, f, 1.0f});
  auto p2f = to_vec3(inv_view * Vec4f{xf, yf, f, 1.0f});
  auto p3f = to_vec3(inv_view * Vec4f{-xf, yf, f, 1.0f});

  vk::debug::draw_line(p0n, p1n, color);
  vk::debug::draw_line(p1n, p2n, color);
  vk::debug::draw_line(p2n, p3n, color);
  vk::debug::draw_line(p3n, p0n, color);

  vk::debug::draw_line(p0f, p1f, color);
  vk::debug::draw_line(p1f, p2f, color);
  vk::debug::draw_line(p2f, p3f, color);
  vk::debug::draw_line(p3f, p0f, color);

  vk::debug::draw_line(p0n, p0f, color);
  vk::debug::draw_line(p1n, p1f, color);
  vk::debug::draw_line(p2n, p2f, color);
  vk::debug::draw_line(p3n, p3f, color);
}

void vk::debug::draw_two_sided_triangles(const Vec3f* p, int num_vertices, const Vec3f& color) {
  assert((num_vertices % 3) == 0);
  if (!render_context.initialized || !render_context.simple_shape_renderer) {
    return;
  }

  render_context.simple_shape_renderer->push_two_sided_triangles(p, num_vertices, color);
}

GROVE_NAMESPACE_END

