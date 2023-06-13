#pragma once

#include "GLResource.hpp"

namespace grove {

class GLRenderContext;
class Vbo;
class Ebo;

namespace gl {
  struct VaoLifecycle {
    static void create(int num, unsigned int* ids);
    static void dispose(int num, unsigned int* ids);
  };
}

class Vao {
  friend class GLRenderContext;
public:
  unsigned int get_instance_handle() const;
  
  void create();
  void configure(GLRenderContext& context, VertexBufferDescriptor* descriptors,
                 const Vbo* vbos, int num_vbos, const Ebo* ebo);
  void configure(GLRenderContext& context, VertexBufferDescriptor** descriptors,
                 const Vbo** vbos, int num_vbos, const Ebo* ebo);
  void dispose();
  
  bool is_valid() const;
  
private:
  void bind() const;
  void unbind() const;
  
private:
  GLResource<gl::VaoLifecycle> instance;
};

}