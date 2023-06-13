#pragma once

#include "GLTexture.hpp"

namespace grove {

class GLTexture3 : public GLTexture {
public:
  GLTexture3();
  explicit GLTexture3(int width_height_depth);
  GLTexture3(int width, int height, int depth);

  GROVE_DELETE_COPY_CTOR_AND_ASSIGNMENT(GLTexture3)
  GROVE_DEFAULT_MOVE_CTOR_AND_ASSIGNMENT(GLTexture3)

  //  internal_format: internal representation, and number of components (e.g., GL_R32F)
  //  source_format: abstract representation (e.g., GL_RED)
  void fill(int level, int internal_format, unsigned int source_format,
            unsigned int type, const void* data) const;

private:
  int width;
  int height;
  int depth;
};

}