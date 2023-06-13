#include "render.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

using namespace editor;

namespace {

constexpr int shape_pool_size() {
  return 32;
}

SimpleShapePools make_simple_shape_pools(SimpleShapeRenderer::GeometryHandle geom) {
  return SimpleShapePools{geom, shape_pool_size(), SimpleShapePools::ReleaseEnabled::No,
                          SimpleShapeRenderer::PipelineType::NonOriented};
}

} //  anon

void editor::UIRenderer::initialize(const InitInfo& info) {
  if (auto cube = info.shape_renderer.require_cube(info.context)) {
    cube_pools = make_simple_shape_pools(cube.value());
  }
  if (auto sphere = info.shape_renderer.require_sphere(info.context)) {
    sphere_pools = make_simple_shape_pools(sphere.value());
  }
}

void editor::UIRenderer::begin_update(const DrawContext& context) {
  if (cube_pools.is_valid()) {
    cube_pools.reset(context.shape_renderer);
  }
  if (sphere_pools.is_valid()) {
    sphere_pools.reset(context.shape_renderer);
  }
}

void editor::UIRenderer::draw_cube(const DrawContext& context,
                                   const Vec3f& pos,
                                   const Vec3f& scale,
                                   const Vec3f& color) {
  if (cube_pools.is_valid()) {
    if (auto handle = cube_pools.acquire(
      context.shape_renderer_context, context.shape_renderer)) {
      //
      context.shape_renderer.set_instance_params(
        handle.value().drawable_handle,
        handle.value().instance_index,
        color,
        scale,
        pos);
    }
  }
}

void editor::UIRenderer::draw_sphere(const DrawContext& context,
                                     const Vec3f& pos,
                                     const Vec3f& scale,
                                     const Vec3f& color) {
  if (sphere_pools.is_valid()) {
    if (auto handle = sphere_pools.acquire(
      context.shape_renderer_context, context.shape_renderer)) {
      //
      context.shape_renderer.set_instance_params(
        handle.value().drawable_handle,
        handle.value().instance_index,
        color,
        scale,
        pos);
    }
  }
}

GROVE_NAMESPACE_END
