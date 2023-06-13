#pragma once

#include "grove/math/Vec3.hpp"
#include "grove/math/Mat4.hpp"
#include "GLTexture.hpp"

namespace grove {

class GLTextureCube : public GLTexture {
public:
  using ViewAxes = std::array<Vec3f, 12>;
public:
  GLTextureCube();
  explicit GLTextureCube(int width_height);
  GLTextureCube(int width, int height);

  GROVE_DELETE_COPY_CTOR_AND_ASSIGNMENT(GLTextureCube)
  GROVE_DEFAULT_MOVE_CTOR_AND_ASSIGNMENT(GLTextureCube)

  void fill(unsigned int face, int level, int internal_format, unsigned int source_format,
            unsigned int type, const void* data = nullptr) const;
  void fill_faces(int level, int internal_format, unsigned int source_format, unsigned int type,
             const void** data = nullptr) const;

  int get_width() const;
  int get_height() const;

  static unsigned int face_index(unsigned int i);
  static ViewAxes view_axes();
  static Mat4f make_view(const ViewAxes& axes, int face_index, const Vec3f& position);

private:
  int face_width;
  int face_height;
};

}