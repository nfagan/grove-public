#pragma once

#include "types.hpp"
#include "Vao.hpp"
#include "GLBuffer.hpp"

namespace grove {

class VertexBufferDescriptor;
class GLRenderContext;

class VertexBufferArray {
public:
  void create(GLRenderContext& context, VertexBufferDescriptor* descriptors,
              int num_descriptors, bool has_ebo = false);
  void dispose();
  bool is_valid() const;
  int num_vbos() const;
  
public:
  Vao vao;
  std::vector<Vbo> vbos;
  Ebo ebo;
};

}