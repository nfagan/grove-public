#pragma once

#include "../render/SimpleShapePools.hpp"

namespace grove::editor {

class UIRenderer {
public:
  struct InitInfo {
    const SimpleShapeRenderer::AddResourceContext& context;
    SimpleShapeRenderer& shape_renderer;
  };
  struct DrawContext {
    const SimpleShapeRenderer::AddResourceContext& shape_renderer_context;
    SimpleShapeRenderer& shape_renderer;
  };

public:
  void initialize(const InitInfo& info);
  void begin_update(const DrawContext& context);
  void draw_cube(const DrawContext& context,
                 const Vec3f& pos,
                 const Vec3f& scale,
                 const Vec3f& color);
  void draw_sphere(const DrawContext& context,
                   const Vec3f& pos,
                   const Vec3f& scale,
                   const Vec3f& color);
private:
  SimpleShapePools cube_pools;
  SimpleShapePools sphere_pools;
};

}