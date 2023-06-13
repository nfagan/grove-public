#pragma once

#include "GLTexture.hpp"
#include "types.hpp"
#include "grove/common/common.hpp"

namespace grove {

class Tbo;

class GLBufferTexture : public GLTexture {
public:
  GLBufferTexture();

  GROVE_DELETE_COPY_CTOR_AND_ASSIGNMENT(GLBufferTexture)
  GROVE_DEFAULT_MOVE_CTOR_AND_ASSIGNMENT(GLBufferTexture)
  
  void set_buffer(const Tbo& tbo, TextureFormat format) const;
  void set_buffer(const Tbo& tbo, unsigned int format) const;
};

}