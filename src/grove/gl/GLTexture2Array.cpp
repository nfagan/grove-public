#include "GLTexture2Array.hpp"
#include <glad/glad.h>
#include <cassert>

namespace grove {

GLTexture2Array::GLTexture2Array() : GLTexture2Array(0, 0, 0) {
  //
}

GLTexture2Array::GLTexture2Array(int width_height, int depth) :
GLTexture2Array(width_height, width_height, depth) {
  //
}

GLTexture2Array::GLTexture2Array(int width, int height, int depth) :
GLTexture(GL_TEXTURE_2D_ARRAY), width(width), height(height), depth(depth) {
  //
}

void GLTexture2Array::fill(int level, int internal_format, unsigned int source_format,
                           unsigned int type, const void* data) const {
  assert(is_valid() && "Invalid texture.");
  glTexImage3D(target, level, internal_format, width, height, depth, 0, source_format, type, data);
}

void GLTexture2Array::set_border_color(const float* rgba) const {
  glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, rgba);
}

int GLTexture2Array::get_width() const {
  return width;
}

int GLTexture2Array::get_height() const {
  return height;
}

int GLTexture2Array::get_depth() const {
  return depth;
}

}
