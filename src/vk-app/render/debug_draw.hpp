#pragma once

#include "grove/math/Vec3.hpp"
#include "grove/math/OBB3.hpp"
#include "grove/math/Bounds3.hpp"

namespace grove {
class SimpleShapeRenderer;

template <typename T>
struct Vec2;

template <typename T>
struct Mat4;
}

namespace grove::vk {

class PointBufferRenderer;
struct Core;
class Allocator;
class BufferSystem;
class StagingBufferSystem;
class CommandProcessor;

}

namespace grove::vk::debug {

void initialize_rendering(PointBufferRenderer* pb_renderer,
                          SimpleShapeRenderer* simple_shape_renderer,
                          const Core* core,
                          Allocator* allocator,
                          BufferSystem* buffer_system,
                          StagingBufferSystem* staging_buffer_system,
                          CommandProcessor* command_processor,
                          uint32_t frame_queue_depth);
void reset_rendering();
void draw_line(const Vec3f& p0, const Vec3f& p1, const Vec3f& color);
void draw_lines(const Vec3f* p, int num_points, const Vec3f& color);
void draw_connected_line(const Vec3f* p, int num_points, const Vec3f& color,
                         bool wrap_around = false);
void draw_triangle_edges(const Vec3f& p0, const Vec3f& p1, const Vec3f& p2, const Vec3f& color);
void draw_triangle_edges(const uint32_t* ti, uint32_t num_triangles,
                         const Vec3f* ps, const Vec3f& color,
                         const Vec3f& scale = Vec3f{1.0f},
                         const Vec3f& offset = Vec3f{});
void draw_obb3(const OBB3f& obb, const Vec3f& color);
void draw_aabb3(const Bounds3f& aabb, const Vec3f& color);
void draw_cube(const Vec3f& p, const Vec3f& s, const Vec3f& color);
void draw_plane(const Vec3f& p, const Vec3f& n, const Vec2<float>& s, const Vec3f& color);
void draw_plane(const Vec3f& p, const Vec3f& x, const Vec3f& y, const Vec2<float>& s,
                const Vec3f& color);
void draw_frustum_lines(float s, float g, float n, float f, const Mat4<float>& inv_view,
                        const Vec3f& color);
void draw_two_sided_triangles(const Vec3f* p, int num_vertices, const Vec3f& color);

}