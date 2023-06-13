#include "GLRenderbuffer.hpp"
#include <glad/glad.h>
#include <cassert>

GROVE_NAMESPACE_BEGIN

void gl::GLRenderbufferLifecycle::create(int number, unsigned int* ids) {
  glGenRenderbuffers(number, ids);
}

void gl::GLRenderbufferLifecycle::dispose(int number, unsigned int* ids) {
  glDeleteRenderbuffers(number, ids);
}

void GLRenderbuffer::create() {
  assert(!instance.is_created && "Renderbuffer already created.");
  instance.create();
}

void GLRenderbuffer::dispose() {
  instance.dispose();
}

bool GLRenderbuffer::is_valid() const {
  return instance.is_created;
}

void GLRenderbuffer::bind() const {
  assert(instance.is_created && "Expected renderbuffer to be created.");
  glBindRenderbuffer(GL_RENDERBUFFER, instance.handle);
}

void GLRenderbuffer::unbind() const {
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

unsigned int GLRenderbuffer::get_instance_handle() const {
  return instance.handle;
}

void GLRenderbuffer::depth_storage(int width, int height, int samples) const {
  storage(GL_DEPTH_COMPONENT, width, height, samples);
}

void GLRenderbuffer::storage(unsigned int internal_format, int width, int height, int samples) const {
  if (samples == 0) {
    glRenderbufferStorage(GL_RENDERBUFFER, internal_format, width, height);
  } else {
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, internal_format, width, height);
  }
}

GROVE_NAMESPACE_END