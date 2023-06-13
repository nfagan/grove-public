#include "GLTexture3.hpp"
#include <cassert>
#include <glad/glad.h>

GROVE_NAMESPACE_BEGIN

GLTexture3::GLTexture3() : GLTexture3(0, 0, 0) {
  //
}

GLTexture3::GLTexture3(int whd) : GLTexture3(whd, whd, whd) {
  //
}

GLTexture3::GLTexture3(int width, int height, int depth) :
  GLTexture(GL_TEXTURE_3D),
  width{width},
  height{height},
  depth{depth} {
  //
}

void GLTexture3::fill(int level, int internal_format, unsigned int source_format,
                      unsigned int type, const void* data) const {
  assert(is_valid() && "Invalid texture.");
  glTexImage3D(target, level, internal_format, width, height, depth,
               0, source_format, type, data);
}

GROVE_NAMESPACE_END
