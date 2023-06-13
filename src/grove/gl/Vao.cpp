#include "Vao.hpp"
#include "GLBuffer.hpp"
#include "GLRenderContext.hpp"
#include "grove/common/common.hpp"
#include <glad/glad.h>
#include <cassert>

GROVE_NAMESPACE_BEGIN

void gl::VaoLifecycle::create(int num, unsigned int* ids) {
  glGenVertexArrays(num, ids);
}

void gl::VaoLifecycle::dispose(int num, unsigned int* ids) {
  glDeleteVertexArrays(num, ids);
}

void Vao::bind() const {
  assert(is_valid() && "Invalid vao.");
  glBindVertexArray(instance.handle);
}

void Vao::unbind() const {
  assert(is_valid() && "Invalid vao.");
  glBindVertexArray(0);
}

void Vao::dispose() {
  instance.dispose();
}

void Vao::configure(GLRenderContext& context, VertexBufferDescriptor* descriptors,
                    const Vbo* vbos, int num_vbos, const Ebo* ebo) {
  assert(is_valid());
  context.bind_vao(*this);

  for (int i = 0; i < num_vbos; i++) {
    vbos[i].bind();
    gl::configure_vertex_attribute_pointers(descriptors[i]);
  }

  if (ebo) {
    ebo->bind();
  }

  context.unbind_vao(*this);
}

void Vao::configure(GLRenderContext& context, VertexBufferDescriptor** descriptors,
                    const Vbo** vbos, int num_vbos, const Ebo* ebo) {
  assert(is_valid());
  context.bind_vao(*this);

  for (int i = 0; i < num_vbos; i++) {
    vbos[i]->bind();
    gl::configure_vertex_attribute_pointers(*descriptors[i]);
  }

  if (ebo) {
    ebo->bind();
  }

  context.unbind_vao(*this);
}

void Vao::create() {
  instance.create();
}

bool Vao::is_valid() const {
  return instance.is_created;
}

unsigned int Vao::get_instance_handle() const {
  assert(is_valid() && "Invalid vao.");
  return instance.handle;
}

GROVE_NAMESPACE_END
