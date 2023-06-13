#pragma once

#include "GLTexture.hpp"

namespace grove {

class GLTexture2Array : public GLTexture {
public:
  GLTexture2Array();
  explicit GLTexture2Array(int width_height, int depth);
  GLTexture2Array(int width, int height, int depth);

  void fill(int level, int internal_format, unsigned int source_format,
            unsigned int type, const void* data) const;

  void set_border_color(const float* rgba) const;

  int get_width() const;
  int get_height() const;
  int get_depth() const;

  GROVE_DELETE_COPY_CTOR_AND_ASSIGNMENT(GLTexture2Array)
  GROVE_DEFAULT_MOVE_CTOR_AND_ASSIGNMENT(GLTexture2Array)

private:
  int width;
  int height;
  int depth;
};

}