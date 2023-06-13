#include "GLTexture2.hpp"
#include "types.hpp"
#include "grove/common/common.hpp"
#include <glad/glad.h>
#include <cassert>

GROVE_NAMESPACE_BEGIN

GLTexture2::GLTexture2() : GLTexture2(0, 0) {
  //
}

GLTexture2::GLTexture2(int width, int height) :
  GLTexture(GL_TEXTURE_2D), width(width), height(height) {
  //
}

GLTexture2::GLTexture2(int width_height) :
  grove::GLTexture2(width_height, width_height) {
  //
}

void GLTexture2::set_dimensions(int w, int h) {
  width = w;
  height = h;
}

void GLTexture2::set_dimensions(int width_height) {
  set_dimensions(width_height, width_height);
}

int GLTexture2::get_width() const {
  return width;
}

int GLTexture2::get_height() const {
  return height;
}

void GLTexture2::set_border_color(const float* rgba) const {
  glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, rgba);
}

void GLTexture2::fill(int level,
                      grove::TextureFormat internal_format,
                      grove::TextureFormat source_format,
                      grove::IntegralType type,
                      const void* data) const {
  const int gl_internal_format = int(gl::texture_format(internal_format));
  const unsigned int gl_source_format = gl::texture_format(source_format);
  const unsigned int gl_integral_type = gl::integral_type(type);
  
  fill(level, gl_internal_format, gl_source_format, gl_integral_type, data);
}

void GLTexture2::fill(int level,
                      int internal_format,
                      unsigned int source_format,
                      unsigned int type,
                      const void* data) const {
  assert(is_valid() && "Invalid texture.");
  glTexImage2D(target, level, internal_format, width, height, 0, source_format, type, data);
}

void GLTexture2::fill_rgba8(int level, const void* data, bool reverse_upload) const {
  //  https://www.khronos.org/opengl/wiki/Common_Mistakes#Texture_upload_and_pixel_reads
  const auto source_format = reverse_upload ? GL_BGRA : GL_RGBA;
  fill(level, GL_RGBA8, source_format, gl::integral_type(IntegralType::UnsignedByte), data);
}

void GLTexture2::fill8(int level, int num_components, const void* data) const {
  TextureFormat format = from_num_components(num_components);
  fill(level, format, format, IntegralType::UnsignedByte, data);
}

void GLTexture2::fill8_srgb(int level, int num_components, const void* data) const {
  if (num_components == 3) {
    fill(level, GL_SRGB8, GL_RGB, GL_UNSIGNED_BYTE, data);
  } else {
    assert(num_components == 4);
    fill(level, GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, data);
  }
}

void GLTexture2::refill(int level,
                        int x_offset,
                        int y_offset,
                        int width_subset,
                        int height_subset,
                        grove::TextureFormat source_format,
                        grove::IntegralType data_type,
                        const void* data) const {
  const unsigned int gl_source_format = gl::texture_format(source_format);
  const unsigned int gl_integral_type = gl::integral_type(data_type);
  
  refill(level, x_offset, y_offset, width_subset, height_subset,
         gl_source_format, gl_integral_type, data);
}

void GLTexture2::refill(int level,
                        int x_offset,
                        int y_offset,
                        int width_subset,
                        int height_subset,
                        unsigned int source_format,
                        unsigned int data_type,
                        const void* data) const {
  assert(is_valid() && "Invalid texture.");
  glTexSubImage2D(target, level, x_offset, y_offset,
                  width_subset, height_subset, source_format, data_type, data);
}

void GLTexture2::refill(int level, grove::TextureFormat source_format,
                        grove::IntegralType data_type, const void* data) const {
  refill(level, 0, 0, width, height, source_format, data_type, data);
}

void GLTexture2::refill(int level, unsigned int source_format,
                        unsigned int type, const void* data) const {
  refill(level, 0, 0, width, height, source_format, type, data);
}

GROVE_NAMESPACE_END
