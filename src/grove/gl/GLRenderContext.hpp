#pragma once

#include "TextureStack.hpp"
#include <unordered_map>

#define GROVE_ALLOW_ENABLE_DISABLE (0)

namespace grove {
  class GLRenderContext;
  class GLTexture;
  class GLFramebuffer;
  class GLRenderbuffer;
  class Vao;
  class Program;
}

class grove::GLRenderContext {
  friend class RenderStateFrame;
public:
  struct RenderState {
    unsigned int cull_face_mode;
    unsigned int depth_function;
    unsigned int blend_function_src;
    unsigned int blend_function_dst;
    unsigned int polygon_mode;

    int viewport[4];
    float clear_color[4];
    float clear_depth;
    float line_width;
    float point_size;

    bool cull_face_enabled;
    bool blend_enabled;
    bool depth_test_enabled;
  };

  class TextureFrame {
  public:
    explicit TextureFrame(GLRenderContext& context);
    ~TextureFrame();
  private:
    GLRenderContext& context;
  };

  class RenderStateFrame {
  public:
    explicit RenderStateFrame(GLRenderContext& context);
    ~RenderStateFrame();

  private:
    GLRenderContext& context;
  };

  using RenderStateStack = std::array<RenderState, 2>;

public:
  GLRenderContext();

  void initialize_render_state();

  void push_texture_frame();
  void pop_texture_frame();
  void set_texture_index(GLTexture& texture);
  int next_free_texture_index(const GLTexture& texture);
  int next_free_texture_index(unsigned int id);

#if GROVE_ALLOW_ENABLE_DISABLE
  void enable(unsigned int feature);
  void disable(unsigned int feature);
#endif

  bool bind_vao(const Vao& vao, bool force = false);
  bool unbind_vao(const Vao& vao, bool force = false);

  bool bind_program(const Program& program, bool force = false);

  bool bind_framebuffer(const GLFramebuffer& framebuffer, bool force = false);
  bool unbind_framebuffer(const GLFramebuffer& framebuffer, bool force = false);

  bool bind_renderbuffer(const GLRenderbuffer& renderbuffer, bool force = false);
  bool unbind_renderbuffer(const GLRenderbuffer& renderbuffer, bool force = false);

  void bind_default_framebuffer();

  void cull_face(unsigned int mode, bool force = false);
  void set_polygon_mode(unsigned int mode, bool force = false);
  void depth_function(unsigned int func, bool force = false);
  void blend_function(unsigned int src, unsigned int dest, bool force = false);
  void viewport(int x, int y, int w, int h, bool force = false);
  void viewport(int* xywh, bool force = false);
  void clear_color(float r, float g, float b, float a, bool force = false);
  void clear_color(float* rgba, bool force = false);
  void clear_depth(float d, bool force = false);
  void clear(int mask);

  void set_cull_face_enabled(bool val, bool force = false);
  void set_blend_enabled(bool val, bool force = false);
  void set_depth_test_enabled(bool val, bool force = false);
  void set_line_width(float val, bool force = false);
  void set_point_size(float val, bool force = false);

  unsigned int check_error() const;
  bool has_error() const;

private:
  void push_render_state();
  void pop_render_state();
  RenderState& current_render_state();

  void maybe_set_enabled_state(bool* target, bool value, unsigned int param, bool force);

#if GROVE_ALLOW_ENABLE_DISABLE
  bool need_change_enabled_state(unsigned int feature, bool enable) const;
#endif

private:
  TextureStack active_textures;

#if GROVE_ALLOW_ENABLE_DISABLE
  std::unordered_map<unsigned int, bool> enabled_state;
#endif

  unsigned int bound_vao;
  unsigned int bound_program;
  unsigned int bound_framebuffer;
  unsigned int bound_renderbuffer;

  RenderStateStack render_state;
  int render_state_size;
};
