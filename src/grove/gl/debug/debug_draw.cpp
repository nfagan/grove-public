#include "debug_draw.hpp"
#include "../ProgramComponent.hpp"
#include "../debug.hpp"
#include "../GLTexture2.hpp"
#include "grove/common/common.hpp"
#include "grove/common/ArrayView.hpp"
#include "grove/common/logging.hpp"
#include "grove/visual/geometry.hpp"
#include "grove/visual/Camera.hpp"
#include "grove/math/matrix_transform.hpp"
#include <glad/glad.h>

GROVE_NAMESPACE_BEGIN

namespace debug {
  class DebugProgram {
  public:
    void initialize(GLRenderContext& context);
    bool is_valid() const;
    void set_view_projection(const Camera& camera) const;
    void set_view_projection(const Mat4f& view, const Mat4f& projection) const;

    void dispose();

  public:
    ProgramComponent program;
  };

  class DebugTexture2Program {
  public:
    void initialize(GLRenderContext& context);
    bool configure(GLRenderContext& context, GLTexture2& texture,
                   const Vec2f& p, const Vec2f& s, int num_components,
                   const DrawTextureParams& params) const;

    bool is_valid() const {
      return program4.is_valid() && program3.is_valid();
    }
    void dispose() {
      program4.dispose();
      program3.dispose();
    }

  public:
    ProgramComponent program4;
    ProgramComponent program3;
  };

  class Quad {
  public:
    bool is_valid() const {
      return draw_component.is_valid();
    }
    void initialize(GLRenderContext& context);
    void dispose() {
      draw_component.dispose();
    }

  public:
    DrawComponent draw_component;
  };

  class Cube {
  public:
    void initialize(GLRenderContext& context);
    bool is_valid() const;
    void dispose();

  public:
    DrawComponent draw_component;
  };

  class Sphere {
  public:
    static constexpr int vertex_dim = 64;
  public:
    void initialize(GLRenderContext& context);
    bool is_valid() const;
    void dispose();

  public:
    DrawComponent draw_component;
  };

  class LineSegmentArray {
  public:
    void dispose();
    void reserve(GLRenderContext& context, const float* positions, int num_points);

  public:
    DrawComponent draw_component;
    int max_num_points{0};
  };
}

namespace globals {

namespace {
  bool debug_drawing_is_initialized = false;
  debug::DebugProgram debug_program;
  debug::DebugTexture2Program debug_texture2_program;
  debug::Cube cube_drawable;
  debug::Sphere sphere_drawable;
  debug::LineSegmentArray line_segment_drawable;
  debug::Quad quad_drawable;
  GLRenderContext* render_context = nullptr;

  const char* const vert_source = R"(
#version 410 core
layout (location = 0) in vec3 position;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
  gl_Position = projection * view * model * vec4(position, 1.0);
}
)";

  const char* const frag_source = R"(
#version 410 core

out vec4 frag_color;
uniform vec3 color;

void main() {
  frag_color = vec4(color, 1.0);
}
)";

  const char* const texture2_vert_source = R"(
#version 410 core

layout (location = 0) in vec2 position;

out vec2 v_uv;

uniform vec2 translation;
uniform vec2 scale;

void main() {
  v_uv = position * 0.5 + 0.5;

  vec2 t = (translation - vec2(0.5)) * 2.0;
  vec2 p = position * scale * 0.5;
  p += t;

  gl_Position = vec4(p.x, p.y, 1.0, 1.0);
}

)";

  std::string make_texture2_fragment_source(int num_components) {
    const char* const base_source = R"(
#version 410 core

out vec4 frag_color;
in vec2 v_uv;
uniform sampler2D color_texture;
uniform int normalize;
uniform vec4 normalize_min;
uniform vec4 normalize_max;

void main() {
)";
    std::string src{base_source};

    if (num_components == 1) {
      src += "frag_color = vec4(texture(color_texture, v_uv).r, 1.0, 1.0, 1.0);";
    } else if (num_components == 2) {
      src += "frag_color = vec4(texture(color_texture, v_uv).rg, 1.0, 1.0);";
    } else if (num_components == 3) {
      src += "frag_color = vec4(texture(color_texture, v_uv).rgb, 1.0);";
    } else {
      assert(num_components == 4);
      src += "frag_color = texture(color_texture, v_uv);";
    }

    src += R"(
if (normalize == 1) {
  frag_color = (frag_color - normalize_min) / (normalize_max - normalize_min);
}
      )";

    src += "\n}";
    return src;
  }
}

}

/*
 * DebugProgram
 */

void debug::DebugProgram::initialize(GLRenderContext& context) {
  bool success;
  program.program =
    grove::make_program(globals::vert_source, globals::frag_source, &success);

  if (!success) {
    GROVE_LOG_ERROR_CAPTURE_META("Failed to create debug program.", "DebugProgram");
  } else {
    program.gather_locations(context);
  }
}

bool debug::DebugProgram::is_valid() const {
  return program.is_valid();
}

void debug::DebugProgram::set_view_projection(const Camera& camera) const {
  set_view_projection(camera.get_view(), camera.get_projection());
}

void debug::DebugProgram::set_view_projection(const Mat4f& view, const Mat4f& projection) const {
  program.set("view", view);
  program.set("projection", projection);
}

void debug::DebugProgram::dispose() {
  program.dispose();
}

/*
 * DebugTexture2Program
 */

void debug::DebugTexture2Program::initialize(GLRenderContext& context) {
  bool success;
  auto frag_source4 = globals::make_texture2_fragment_source(4);
  auto frag_source3 = globals::make_texture2_fragment_source(3);

  program4.program = grove::make_program(
    globals::texture2_vert_source, frag_source4.c_str(), &success);

  if (!success) {
    GROVE_LOG_ERROR_CAPTURE_META("Failed to create debug program.", "DebugTexture2Program");
  } else {
    program4.gather_locations(context);
  }

  program3.program = grove::make_program(
    globals::texture2_vert_source, frag_source3.c_str(), &success);
    if (!success) {
      GROVE_LOG_ERROR_CAPTURE_META("Failed to create debug program.", "DebugTexture2Program");
    } else {
      program3.gather_locations(context);
    }
}

bool debug::DebugTexture2Program::configure(GLRenderContext& context,
                                            GLTexture2& texture,
                                            const Vec2f& p,
                                            const Vec2f& s,
                                            int num_components,
                                            const DrawTextureParams& params) const {
  const ProgramComponent* use_prog = nullptr;

  if (num_components == 4) {
    use_prog = &program4;
  } else if (num_components == 3) {
    use_prog = &program3;
  }

  if (!use_prog) {
    return false;
  }

  auto& prog = use_prog->program;
  use_prog->bind(context);
  use_prog->set("color_texture", texture);
  use_prog->set("translation", p);
  use_prog->set("scale", s);
  use_prog->set("normalize", int(params.normalize));
  prog.set_float4(prog.uniform_location("normalize_min"),
                  params.min[0], params.min[1], params.min[2], params.min[3]);
  prog.set_float4(prog.uniform_location("normalize_max"),
                  params.max[0], params.max[1], params.max[2], params.max[3]);

  return true;
}

/*
 * Cube
 */

bool debug::Cube::is_valid() const {
  return draw_component.is_valid();
}

void debug::Cube::dispose() {
  draw_component.dispose();
}

void debug::Cube::initialize(GLRenderContext& context) {
  const auto pos = geometry::cube_positions();
  const auto inds = geometry::cube_indices();

  VertexBufferDescriptor buffer_descriptor;
  buffer_descriptor.add_attribute(AttributeDescriptor::float3(0));

  auto draw_descriptor =
    DrawDescriptor::elements(DrawMode::Triangles, inds.size(), IntegralType::UnsignedShort);

  draw_component.initialize(context, draw_descriptor, buffer_descriptor, pos, inds);
}

/*
 * Sphere
 */

bool debug::Sphere::is_valid() const {
  return draw_component.is_valid();
}

void debug::Sphere::dispose() {
  draw_component.dispose();
}

void debug::Sphere::initialize(GLRenderContext& context) {
  const auto pos = geometry::triangle_strip_sphere_data(vertex_dim, false, false);
  const auto inds = geometry::triangle_strip_indices(vertex_dim);

  VertexBufferDescriptor buffer_descriptor;
  buffer_descriptor.add_attribute(AttributeDescriptor::float3(0));

  auto draw_descriptor =
    DrawDescriptor::elements(DrawMode::TriangleStrip, inds.size(), IntegralType::UnsignedShort);

  draw_component.initialize(context, draw_descriptor, buffer_descriptor, pos, inds);
}

/*
 * LineSegmentArray
 */

void debug::LineSegmentArray::dispose() {
  draw_component.dispose();
}

void debug::LineSegmentArray::reserve(GLRenderContext& context,
                                      const float* positions,
                                      int num_points) {
  auto draw_descriptor = DrawDescriptor::arrays(DrawMode::Lines, num_points);

  if (num_points > max_num_points || !draw_component.is_valid()) {
    VertexBufferDescriptor buffer_descriptor;
    buffer_descriptor.add_attribute(AttributeDescriptor::float3(0));

    DrawComponent component;
    component.initialize(context, draw_descriptor, buffer_descriptor,
                         positions, num_points * 3 * sizeof(float));

    draw_component = std::move(component);
    max_num_points = num_points;

  } else {
    assert(draw_component.vertex_array.vbos.size() == 1);
    auto& pos_vbo = draw_component.vertex_array.vbos[0];

    pos_vbo.bind_fill(positions, num_points * 3 * sizeof(float));
    draw_component.draw_descriptor = draw_descriptor;
  }
}

/*
 * Quad
 */

void debug::Quad::initialize(GLRenderContext& context) {
  VertexBufferDescriptor buffer_descriptor;
  buffer_descriptor.add_attribute(AttributeDescriptor::float2(0));

  const auto positions = geometry::quad_positions(false);
  const auto inds = geometry::quad_indices();

  auto draw_descriptor = DrawDescriptor::elements(
    DrawMode::Triangles, inds.size(), IntegralType::UnsignedShort);

  draw_component.initialize(context, draw_descriptor, buffer_descriptor,
                            positions.data(), positions.size() * sizeof(float),
                            inds.data(), inds.size() * sizeof(uint16_t));
}

/*
 * util
 */

void debug::initialize_debug_rendering(GLRenderContext* context) {
  assert(!globals::debug_drawing_is_initialized);
  globals::debug_program.initialize(*context);
  globals::debug_texture2_program.initialize(*context);
  globals::cube_drawable.initialize(*context);
  globals::sphere_drawable.initialize(*context);
  globals::quad_drawable.initialize(*context);
  globals::render_context = context;

  globals::debug_drawing_is_initialized = true;
}

void debug::terminate_debug_rendering() {
  if (!globals::debug_drawing_is_initialized) {
    return;
  }

  globals::cube_drawable.dispose();
  globals::sphere_drawable.dispose();
  globals::quad_drawable.dispose();
  globals::debug_program.dispose();
  globals::debug_texture2_program.dispose();

  globals::render_context = nullptr;
  globals::debug_drawing_is_initialized = false;
}

namespace {

void render_draw_component(DrawComponent& draw_component,
                           const Mat4f& model,
                           const Mat4f& view,
                           const Mat4f& projection,
                           const Vec3f& color) {
  assert(globals::render_context);

  globals::debug_program.program.bind(*globals::render_context);
  globals::debug_program.set_view_projection(view, projection);
  globals::debug_program.program.set("model", model);
  globals::debug_program.program.set("color", color);

  draw_component.bind_vao(*globals::render_context);
  draw_component.draw();
}

} //  anon

/*
 * draw_cube
 */

void debug::draw_cube(const Mat4f& model, const Camera& camera, const Vec3f& color) {
  draw_cube(model, camera.get_view(), camera.get_projection(), color);
}

void debug::draw_cube(const Vec3f& position, const Camera& camera, const Vec3f& color) {
  auto model = make_translation(position);
  draw_cube(model, camera, color);
}

void debug::draw_cube(const Mat4f& model,
                      const Mat4f& view,
                      const Mat4f& projection,
                      const Vec3f& color) {
  assert(globals::render_context);
  auto& draw_component = globals::cube_drawable.draw_component;
  render_draw_component(draw_component, model, view, projection, color);
}

/*
 * draw_sphere
 */

void debug::draw_sphere(const Mat4f& model, const Camera& camera, const Vec3f& color) {
  draw_sphere(model, camera.get_view(), camera.get_projection(), color);
}

void debug::draw_sphere(const Vec3f& position, const Camera& camera, const Vec3f& color) {
  auto model = make_translation(position);
  draw_sphere(model, camera, color);
}

void debug::draw_sphere(const Mat4f& model,
                        const Mat4f& view,
                        const Mat4f& projection,
                        const Vec3f& color) {
  assert(globals::render_context);

  GLRenderContext::RenderStateFrame render_state_frame(*globals::render_context);
  globals::render_context->cull_face(GL_FRONT);

  auto& draw_component = globals::sphere_drawable.draw_component;
  render_draw_component(draw_component, model, view, projection, color);
}

void debug::draw_lines(const ArrayView<const float>& ps,
                       const Camera& camera,
                       const Vec3f& color) {
  assert(globals::render_context);

  auto num_points = int(ps.size() / 3);
  globals::line_segment_drawable.reserve(*globals::render_context, ps.begin(), num_points);

  auto& draw_component = globals::line_segment_drawable.draw_component;
  auto view = camera.get_view();
  auto proj = camera.get_projection();
  Mat4f model{1.0f};

  render_draw_component(draw_component, model, view, proj, color);
}

void debug::draw_texture2(GLTexture2& texture,
                          const GLRenderContext::TextureFrame& texture_frame,
                          const Vec2f& pos,
                          const Vec2f& size,
                          int num_color_components,
                          const DrawTextureParams& params) {
  assert(globals::render_context &&
         globals::debug_texture2_program.is_valid() &&
         globals::quad_drawable.is_valid());

  auto& ctx = *globals::render_context;
  (void) texture_frame;

  ctx.set_texture_index(texture);
  texture.activate_bind();

  bool can_draw = globals::debug_texture2_program.configure(
    ctx, texture, pos, size, num_color_components, params);

  if (can_draw) {
    globals::quad_drawable.draw_component.bind_vao(ctx);
    globals::quad_drawable.draw_component.draw();
  }
}

GROVE_NAMESPACE_END
