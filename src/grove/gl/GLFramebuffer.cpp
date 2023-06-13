#include "GLFramebuffer.hpp"
#include "GLTexture2.hpp"
#include "GLTexture2Array.hpp"
#include "GLRenderbuffer.hpp"
#include "context.hpp"
#include <glad/glad.h>
#include <cassert>

namespace grove {

namespace globals {

constexpr int max_num_default_color_attachments = 16;
unsigned int default_color_attachments[max_num_default_color_attachments];
bool set_default_color_attachments{};

void require_default_color_attachments() {
  if (!set_default_color_attachments) {
    for (int i = 0; i < max_num_default_color_attachments; i++) {
      default_color_attachments[i] = GL_COLOR_ATTACHMENT0 + i;
    }
    set_default_color_attachments = true;
  }
}

} //  globals

void gl::FramebufferLifecycle::create(int number, unsigned int* ids) {
  glGenFramebuffers(number, ids);
}

void gl::FramebufferLifecycle::dispose(int number, unsigned int* ids) {
  glDeleteFramebuffers(number, ids);
}

void GLFramebuffer::create() {
  assert(!is_created() && "Framebuffer was already created.");
  instance.create();
}

void GLFramebuffer::dispose() {
  instance.dispose();
}

bool GLFramebuffer::is_valid() const {
  return is_created() && is_complete();
}

bool GLFramebuffer::is_complete() const {
  if (!is_created()) {
    return false;
  }

  int currently_bound_fbo;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currently_bound_fbo);
  //  Check whether a different fbo is active, and if so, make sure to re-bind the original after querying the status.
  const bool need_bind = static_cast<unsigned int>(currently_bound_fbo) != instance.handle;

  if (need_bind) {
    glBindFramebuffer(GL_FRAMEBUFFER, instance.handle);
  }

  const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

  if (need_bind) {
    glBindFramebuffer(GL_FRAMEBUFFER, currently_bound_fbo);
  }

  return complete;
}

bool GLFramebuffer::is_created() const {
  return instance.is_created;
}

unsigned int GLFramebuffer::get_instance_handle() const {
  return instance.handle;
}

void GLFramebuffer::bind() const {
  assert(is_created() && "Framebuffer was not created.");
  glBindFramebuffer(GL_FRAMEBUFFER, instance.handle);
}

void GLFramebuffer::unbind() const {
  assert(is_created() && "Framebuffer was not created.");
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLFramebuffer::attach_texture2(unsigned int attachment, unsigned int texture_target,
                                    unsigned int texture_handle) const {
  assert(is_created() && "Framebuffer was not created.");
  glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, texture_target, texture_handle, 0);
}

void GLFramebuffer::attach_texture2(unsigned int attachment, const grove::GLTexture2& texture) const {
  attach_texture2(attachment, GL_TEXTURE_2D, texture.get_instance_handle());
}

void GLFramebuffer::attach_texture2_array(unsigned int attachment,
                                          const GLTexture2Array& texture,
                                          int level, int layer) const {
  glFramebufferTextureLayer(GL_FRAMEBUFFER, attachment, texture.get_instance_handle(), level, layer);
}

void GLFramebuffer::attach_renderbuffer(unsigned int attachment, const grove::GLRenderbuffer& renderbuffer) const {
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, renderbuffer.get_instance_handle());
}

void GLFramebuffer::attach_depth_renderbuffer(const GLRenderbuffer& renderbuffer) const {
  attach_renderbuffer(GL_DEPTH_ATTACHMENT, renderbuffer);
}

void GLFramebuffer::set_draw_buffers(int count, const unsigned int* attachments) {
  glDrawBuffers(count, attachments);
}

void GLFramebuffer::set_color_attachment_draw_buffers_range(int size) const {
  assert(size < globals::max_num_default_color_attachments &&
         size < gl::max_num_fbo_color_attachments());
  globals::require_default_color_attachments();
  glDrawBuffers(size, globals::default_color_attachments);
}

}
