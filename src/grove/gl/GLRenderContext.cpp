#include "GLRenderContext.hpp"
#include "GLTexture.hpp"
#include "GLFramebuffer.hpp"
#include "GLRenderbuffer.hpp"
#include "Program.hpp"
#include "Vao.hpp"
#include "debug.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include <glad/glad.h>
#include <cassert>
#include <iostream>

GROVE_NAMESPACE_BEGIN

/*
 * GLRenderContext::TextureFrame
 */

GLRenderContext::TextureFrame::TextureFrame(GLRenderContext& context) :
  context(context) {
  //
  context.push_texture_frame();
}

GLRenderContext::TextureFrame::~TextureFrame() {
  context.pop_texture_frame();
}

/*
 * GLRenderContext::RenderStateFrame
 */

GLRenderContext::RenderStateFrame::RenderStateFrame(GLRenderContext& context) :
  context(context) {
  //
  context.push_render_state();
}

GLRenderContext::RenderStateFrame::~RenderStateFrame() {
  context.pop_render_state();
}

/*
 * GLRenderContext
 */

GLRenderContext::GLRenderContext() :
  bound_vao(0),
  bound_program(0),
  bound_framebuffer(0),
  bound_renderbuffer(0),
  render_state{},
  render_state_size(1) {
  //
}

void GLRenderContext::push_texture_frame() {
  active_textures.push_texture_frame();
}

void GLRenderContext::pop_texture_frame() {
  active_textures.pop_texture_frame();
}

void GLRenderContext::set_texture_index(GLTexture& texture) {
  texture.set_index(active_textures.next_free_index(texture.get_id()));
}

int GLRenderContext::next_free_texture_index(const grove::GLTexture& texture) {
  return active_textures.next_free_index(texture.get_id());
}

int GLRenderContext::next_free_texture_index(unsigned int id) {
  return active_textures.next_free_index(id);
}

#if GROVE_ALLOW_ENABLE_DISABLE
void GLRenderContext::disable(unsigned int feature) {
  if (need_change_enabled_state(feature, false)) {
    glDisable(feature);
    enabled_state[feature] = false;
  }
}

void GLRenderContext::enable(unsigned int feature) {
  if (need_change_enabled_state(feature, true)) {
    glEnable(feature);
    enabled_state[feature] = true;
  }
}
#endif

bool GLRenderContext::bind_vao(const Vao& vao, bool force) {
  const unsigned int instance_handle = vao.get_instance_handle();

  if (force || bound_vao != instance_handle) {
    vao.bind();
    bound_vao = instance_handle;
    return true;
  } else {
    return false;
  }
}

bool GLRenderContext::unbind_vao(const grove::Vao& vao, bool force) {
  const unsigned int instance_handle = vao.get_instance_handle();
  
  if (force || bound_vao == instance_handle) {
    vao.unbind();
    bound_vao = 0;
    return true;
  } else {
    return false;
  }
}

bool GLRenderContext::bind_program(const Program& prog, bool force) {
  const unsigned int instance_handle = prog.get_instance_handle();

  if (force || bound_program != instance_handle) {
    prog.bind();
    bound_program = instance_handle;
    return true;
  } else {
    return false;
  }
}

bool GLRenderContext::bind_framebuffer(const grove::GLFramebuffer& framebuffer, bool force) {
  const unsigned int instance_handle = framebuffer.get_instance_handle();

  if (force || bound_framebuffer != instance_handle) {
    framebuffer.bind();
    bound_framebuffer = instance_handle;
    return true;
  } else {
    return false;
  }
}

bool GLRenderContext::unbind_framebuffer(const grove::GLFramebuffer& framebuffer, bool force) {
  const unsigned int instance_handle = framebuffer.get_instance_handle();

  if (force || bound_framebuffer == instance_handle) {
    framebuffer.unbind();
    bound_framebuffer = 0;
    return true;
  } else {
    return false;
  }
}

bool GLRenderContext::bind_renderbuffer(const grove::GLRenderbuffer& renderbuffer, bool force) {
  const unsigned int instance_handle = renderbuffer.get_instance_handle();

  if (force || bound_renderbuffer != instance_handle) {
    renderbuffer.bind();
    bound_renderbuffer = instance_handle;
    return true;
  } else {
    return false;
  }
}

bool GLRenderContext::unbind_renderbuffer(const grove::GLRenderbuffer& renderbuffer, bool force) {
  const unsigned int instance_handle = renderbuffer.get_instance_handle();

  if (force || bound_renderbuffer == instance_handle) {
    renderbuffer.unbind();
    bound_renderbuffer = 0;
    return true;
  } else {
    return false;
  }
}

void GLRenderContext::bind_default_framebuffer() {
  bound_framebuffer = 0;
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

#if GROVE_ALLOW_ENABLE_DISABLE
bool GLRenderContext::need_change_enabled_state(unsigned int feature, bool enable) const {
  const auto& it = enabled_state.find(feature);
  return it == enabled_state.end() || it->second != enable;
}
#endif

GLRenderContext::RenderState& GLRenderContext::current_render_state() {
  assert(render_state_size > 0 && render_state_size <= int(render_state.size()));
  return render_state[render_state_size-1];
}

void GLRenderContext::initialize_render_state() {
  //  https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glIsEnabled.xhtml
  //  https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glGet.xhtml
  auto& state0 = render_state[0];

  glGetIntegerv(GL_CULL_FACE_MODE, (GLint*) &state0.cull_face_mode);
  glGetIntegerv(GL_POLYGON_MODE, (GLint*) &state0.polygon_mode);
  glGetIntegerv(GL_DEPTH_FUNC, (GLint*) &state0.depth_function);
  glGetIntegerv(GL_BLEND_SRC_RGB, (GLint*) &state0.blend_function_src);
  glGetIntegerv(GL_BLEND_DST_RGB, (GLint*) &state0.blend_function_dst);
  glGetIntegerv(GL_VIEWPORT, (GLint*) &state0.viewport[0]);
  glGetFloatv(GL_COLOR_CLEAR_VALUE, (GLfloat*) &state0.clear_color[0]);
  glGetFloatv(GL_DEPTH_CLEAR_VALUE, (GLfloat*) &state0.clear_depth);
  glGetFloatv(GL_LINE_WIDTH, (GLfloat*) &state0.line_width);
  glGetFloatv(GL_POINT_SIZE, (GLfloat*) &state0.point_size);

  state0.cull_face_enabled = glIsEnabled(GL_CULL_FACE);
  state0.blend_enabled = glIsEnabled(GL_BLEND);
  state0.depth_test_enabled = glIsEnabled(GL_DEPTH_TEST);
}

void GLRenderContext::push_render_state() {
  assert(render_state_size < int(render_state.size()));
  render_state[render_state_size] = render_state[render_state_size - 1];
  render_state_size++;
}

void GLRenderContext::pop_render_state() {
  //  Should always have at least 1 render state frame (frame 0).
  assert(render_state_size > 1);

  int curr_ind = render_state_size - 1;
  int prev_ind = render_state_size - 2;
  auto& prev_state = render_state[prev_ind];

  //  Reapply previous state.
  cull_face(prev_state.cull_face_mode);
  set_polygon_mode(prev_state.polygon_mode);
  depth_function(prev_state.depth_function);
  blend_function(prev_state.blend_function_src, prev_state.blend_function_dst);
  viewport(&prev_state.viewport[0]);
  clear_color(&prev_state.clear_color[0]);
  clear_depth(prev_state.clear_depth);
  set_line_width(prev_state.line_width);
  set_point_size(prev_state.point_size);

  set_cull_face_enabled(prev_state.cull_face_enabled);
  set_blend_enabled(prev_state.blend_enabled);
  set_depth_test_enabled(prev_state.depth_test_enabled);

  auto& curr_state = render_state[curr_ind];
  render_state[prev_ind] = curr_state;
  render_state_size--;
}

void GLRenderContext::cull_face(unsigned int mode, bool force) {
  auto& curr_state = current_render_state();
  if (force || curr_state.cull_face_mode != mode) {
    glCullFace(mode);
    curr_state.cull_face_mode = mode;
  }
}

void GLRenderContext::set_polygon_mode(unsigned int mode, bool force) {
  auto& curr_state = current_render_state();
  if (force || curr_state.polygon_mode != mode) {
    glPolygonMode(GL_FRONT_AND_BACK, mode);
    curr_state.polygon_mode = mode;
  }
}

void GLRenderContext::depth_function(unsigned int func, bool force) {
  auto& curr_state = current_render_state();
  if (force || curr_state.depth_function != func) {
    glDepthFunc(func);
    curr_state.depth_function = func;
  }
}

void GLRenderContext::set_line_width(float val, bool force) {
  auto& curr_state = current_render_state();
  if (force || curr_state.line_width != val) {
    glLineWidth(val);
    curr_state.line_width = val;
  }
}

void GLRenderContext::set_point_size(float val, bool force) {
  auto& curr_state = current_render_state();
  if (force || curr_state.point_size != val) {
    glPointSize(val);
    curr_state.point_size = val;
  }
}

void GLRenderContext::blend_function(unsigned int src, unsigned int dst, bool force) {
  auto& curr_state = current_render_state();
  if (force ||
      curr_state.blend_function_src != src ||
      curr_state.blend_function_dst != dst) {
    //
    glBlendFunc(src, dst);
    curr_state.blend_function_src = src;
    curr_state.blend_function_dst = dst;
  }
}

void GLRenderContext::viewport(int x, int y, int w, int h, bool force) {
  auto& curr_state = current_render_state();
  if (force ||
      curr_state.viewport[0] != x ||
      curr_state.viewport[1] != y ||
      curr_state.viewport[2] != w ||
      curr_state.viewport[3] != h) {
    //
    glViewport(x, y, w, h);
    curr_state.viewport[0] = x;
    curr_state.viewport[1] = y;
    curr_state.viewport[2] = w;
    curr_state.viewport[3] = h;
  }
}

void GLRenderContext::viewport(int* xywh, bool force) {
  viewport(xywh[0], xywh[1], xywh[2], xywh[3], force);
}

void GLRenderContext::clear_color(float r, float g, float b, float a, bool force) {
  auto& curr_state = current_render_state();
  if (force ||
      curr_state.clear_color[0] != r ||
      curr_state.clear_color[1] != g ||
      curr_state.clear_color[2] != b ||
      curr_state.clear_color[3] != a) {
    //
    glClearColor(r, g, b, a);
    curr_state.clear_color[0] = r;
    curr_state.clear_color[1] = g;
    curr_state.clear_color[2] = b;
    curr_state.clear_color[3] = a;
  }
}

void GLRenderContext::clear_color(float *rgba, bool force) {
  clear_color(rgba[0], rgba[1], rgba[2], rgba[3], force);
}

void GLRenderContext::clear_depth(float d, bool force) {
  auto& curr_state = current_render_state();
  if (force || curr_state.clear_depth != d) {
    glClearDepth(d);
    curr_state.clear_depth = d;
  }
}

void GLRenderContext::clear(int mask) {
  glClear(mask);
}

void GLRenderContext::maybe_set_enabled_state(bool* target, bool value,
                                              unsigned int param, bool force) {
  if (force || *target != value) {
    if (value) {
      glEnable(param);
    } else {
      glDisable(param);
    }
    *target = value;
  }
}

void GLRenderContext::set_cull_face_enabled(bool val, bool force) {
  auto& curr_state = current_render_state();
  maybe_set_enabled_state(&curr_state.cull_face_enabled, val, GL_CULL_FACE, force);
}

void GLRenderContext::set_depth_test_enabled(bool val, bool force) {
  auto& curr_state = current_render_state();
  maybe_set_enabled_state(&curr_state.depth_test_enabled, val, GL_DEPTH_TEST, force);
}

void GLRenderContext::set_blend_enabled(bool val, bool force) {
  auto& curr_state = current_render_state();
  maybe_set_enabled_state(&curr_state.blend_enabled, val, GL_BLEND, force);
}

unsigned int GLRenderContext::check_error() const {
  auto err = glGetError();
  if (err != GL_NO_ERROR) {
    GROVE_LOG_SEVERE_CAPTURE_META(debug::get_error_code_str(err), "GLRenderContext");
  }
  return err;
}

bool GLRenderContext::has_error() const {
  return glGetError() != GL_NO_ERROR;
}

GROVE_NAMESPACE_END

