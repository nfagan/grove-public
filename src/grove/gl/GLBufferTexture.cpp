#include "GLBufferTexture.hpp"
#include "GLBuffer.hpp"
#include <glad/glad.h>
#include <cassert>

grove::GLBufferTexture::GLBufferTexture() : GLTexture(GL_TEXTURE_BUFFER) {
  //
}

void grove::GLBufferTexture::set_buffer(const grove::Tbo& tbo, grove::TextureFormat format) const {
  set_buffer(tbo, gl::texture_format(format));
}

void grove::GLBufferTexture::set_buffer(const grove::Tbo& tbo, unsigned int format) const {
  assert(is_valid() && "Invalid texture.");
  glTexBuffer(target, format, tbo.get_instance_handle());
}