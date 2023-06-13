#include "GLTexture.hpp"
#include "grove/common/common.hpp"
#include "grove/visual/Texture.hpp"
#include "types.hpp"
#include <glad/glad.h>
#include <cassert>

GROVE_NAMESPACE_BEGIN

void gl::GLTextureLifecycle::create(int num, unsigned int* ids) {
  glGenTextures(num, ids);
}

void gl::GLTextureLifecycle::dispose(int num, unsigned int* ids) {
  glDeleteTextures(num, ids);
}

unsigned long GLTexture::num_instances = 0;

GLTexture::GLTexture(unsigned int target) : target(target), index(0), id(++num_instances) {
  //
}

void GLTexture::create() {
  assert(!is_valid() && "Recreated texture.");
  instance.create();
}

void GLTexture::dispose() {
  instance.dispose();
}

bool GLTexture::is_valid() const {
  return instance.is_created;
}

void GLTexture::set_index(int to) {
  index = to;
}

unsigned long GLTexture::get_id() const {
  return id;
}

int GLTexture::get_index() const {
  return index;
}

unsigned int GLTexture::get_instance_handle() const {
  return instance.handle;
}

void GLTexture::bind() const {
  assert(is_valid() && "Invalid texture.");
  glBindTexture(target, instance.handle);
}

void GLTexture::configure(const grove::TextureParameters& params) const {
  gl::configure_texture(target, params);
}

void GLTexture::bind_configure(const grove::TextureParameters& params) const {
  bind();
  configure(params);
}

void GLTexture::activate() const {
  assert(is_valid() && "Invalid texture.");
  glActiveTexture(GL_TEXTURE0 + index);
}

void GLTexture::activate_bind() const {
  activate();
  bind();
}

void gl::configure_texture(unsigned int target, const grove::TextureParameters& params) {
  if (params.min_filter != TextureFilterMethod::None) {
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, gl::filter_method(params.min_filter));
  }

  if (params.mag_filter != TextureFilterMethod::None) {
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, gl::filter_method(params.mag_filter));
  }

  if (params.wrap_s != TextureWrapMethod::None) {
    glTexParameteri(target, GL_TEXTURE_WRAP_S, gl::wrap_method(params.wrap_s));
  }

  if (params.wrap_t != TextureWrapMethod::None) {
    glTexParameteri(target, GL_TEXTURE_WRAP_T, gl::wrap_method(params.wrap_t));
  }

  if (params.wrap_r != TextureWrapMethod::None) {
    glTexParameteri(target, GL_TEXTURE_WRAP_R, gl::wrap_method(params.wrap_r));
  }
}

GROVE_NAMESPACE_END
