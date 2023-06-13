#pragma once

#include "../DrawComponent.hpp"
#include "grove/math/matrix.hpp"
#include "grove/math/vector.hpp"

namespace grove {

class Camera;
class GLTexture2;

template <typename T>
class ArrayView;

namespace debug {
  struct DrawTextureParams {
    bool normalize{};
    float min[4]{0.0f, 0.0f, 0.0f, 0.0f};
    float max[4]{1.0f, 1.0f, 1.0f, 1.0f};
  };

  void initialize_debug_rendering(GLRenderContext* context);
  void terminate_debug_rendering();

  void draw_cube(const Mat4f& model,
                 const Mat4f& view,
                 const Mat4f& projection,
                 const Vec3f& color);

  void draw_cube(const Mat4f& model, const Camera& camera, const Vec3f& color);
  void draw_cube(const Vec3f& position, const Camera& camera, const Vec3f& color);

  void draw_sphere(const Mat4f& model,
                   const Mat4f& view,
                   const Mat4f& projection,
                   const Vec3f& color);

  void draw_sphere(const Mat4f& model, const Camera& camera, const Vec3f& color);
  void draw_sphere(const Vec3f& position, const Camera& camera, const Vec3f& color);

  void draw_lines(const ArrayView<const float>& ps, const Camera& camera, const Vec3f& color);

  void draw_texture2(GLTexture2& texture,
                     const GLRenderContext::TextureFrame& texture_frame,
                     const Vec2f& pos,
                     const Vec2f& size,
                     int num_color_components,
                     const DrawTextureParams& params = DrawTextureParams{});
}

}