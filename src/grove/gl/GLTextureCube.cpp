#include "GLTextureCube.hpp"
#include <glad/glad.h>
#include <cassert>

#define USE_CANONICAL_ORIENTATION (1)

namespace grove {

GLTextureCube::GLTextureCube() : GLTextureCube(0, 0) {
  //
}

GLTextureCube::GLTextureCube(int width, int height) :
GLTexture(GL_TEXTURE_CUBE_MAP), face_width(width), face_height(height) {
  //
}

GLTextureCube::GLTextureCube(int width_height) : GLTextureCube(width_height, width_height) {
  //
}

void GLTextureCube::fill(unsigned int face, int level, int internal_format, unsigned int source_format,
                         unsigned int type, const void* data) const {
  assert(is_valid() && "Invalid cube map texture.");
  glTexImage2D(face, level, internal_format, face_width, face_height, 0, source_format, type, data);
}

void GLTextureCube::fill_faces(int level, int internal_format, unsigned int source_format,
                          unsigned int type, const void** data) const {
  for (int i = 0; i < 6; i++) {
    const void* data_ptr = data ? data[i] : nullptr;
    fill(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, level, internal_format, source_format, type, data_ptr);
  }
}

unsigned int GLTextureCube::face_index(unsigned int i) {
  return GL_TEXTURE_CUBE_MAP_POSITIVE_X + i;
}

int GLTextureCube::get_width() const {
  return face_width;
}

int GLTextureCube::get_height() const {
  return face_height;
}

GLTextureCube::ViewAxes GLTextureCube::view_axes() {
  GLTextureCube::ViewAxes result;
  int index = 0;

#if USE_CANONICAL_ORIENTATION
  //  +x
  result[index++] = Vec3f{-1.0f, 0.0f, 0.0f};
  result[index++] = Vec3f{0.0f, 1.0f, 0.0f};
  //  -x
  result[index++] = Vec3f{1.0f, 0.0f, 0.0f};
  result[index++] = Vec3f{0.0f, 1.0f, 0.0f};
  //  +y
  result[index++] = Vec3f{0.0f, -1.0f, 0.0f};
  result[index++] = Vec3f{0.0f, 0.0f, -1.0f};
  //  -y
  result[index++] = Vec3f{0.0f, 1.0f, 0.0f};
  result[index++] = Vec3f{0.0f, 0.0f, 1.0f};
  //  +z
  result[index++] = Vec3f{0.0f, 0.0f, -1.0f};
  result[index++] = Vec3f{0.0f, 1.0f, 0.0f};
  //  -z
  result[index++] = Vec3f{0.0f, 0.0f, 1.0f};
  result[index++] = Vec3f{0.0f, 1.0f, 0.0f};
#else
  //  +x
  result[index++] = glm::vec3(-1.0f, 0.0f, 0.0f);
  result[index++] = glm::vec3(0.0f, 1.0f, 0.0f);
  //  -x
  result[index++] = glm::vec3(1.0f, 0.0f, 0.0f);
  result[index++] = glm::vec3(0.0f, 1.0f, 0.0f);
  //  -y
  result[index++] = glm::vec3(0.0f, 1.0f, 0.0f);
  result[index++] = glm::vec3(0.0f, 0.0f, 1.0f);
  //  +y
  result[index++] = glm::vec3(0.0f, -1.0f, 0.0f);
  result[index++] = glm::vec3(0.0f, 0.0f, -1.0f);
  //  +z
  result[index++] = glm::vec3(0.0f, 0.0f, -1.0f);
  result[index++] = glm::vec3(0.0f, 1.0f, 0.0f);
  //  -z
  result[index++] = glm::vec3(0.0f, 0.0f, 1.0f);
  result[index++] = glm::vec3(0.0f, 1.0f, 0.0f);
#endif

  return result;
}

Mat4f GLTextureCube::make_view(const ViewAxes& axes, int face_index, const Vec3f& position) {
  assert(face_index >= 0 && face_index < int(axes.size()));

  auto f = axes[face_index*2];
  auto world_u = axes[face_index*2+1];
  auto r = cross(f, world_u);
#if USE_CANONICAL_ORIENTATION
  auto u = cross(f, r);
#else
  auto u = glm::cross(r, f);
#endif

  r = round(r);
  u = round(u);
  f = round(f);

  Mat4f m{1.0f};
  m[0] = Vec4f{r, 0.0f};
  m[1] = Vec4f{u, 0.0f};
  m[2] = Vec4f{f, 0.0f};
  m = transpose(m);
  m[3] = -(m * Vec4f{position, 1.0f});

  return m;
}

}