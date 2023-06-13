#include "VertexBufferArray.hpp"
#include "GLRenderContext.hpp"
#include "grove/common/common.hpp"
#include <cassert>

GROVE_NAMESPACE_BEGIN

void VertexBufferArray::dispose() {
  for (auto& vbo : vbos) {
    vbo.dispose();
  }

  ebo.dispose();
  vao.dispose();
  vbos.clear();
}

bool VertexBufferArray::is_valid() const {
  return vao.is_valid();
}

int VertexBufferArray::num_vbos() const {
  return int(vbos.size());
}

void VertexBufferArray::create(GLRenderContext& context,
                               VertexBufferDescriptor* vbo_descriptors,
                               int num_vbos,
                               bool has_ebo) {
  assert(!is_valid() && "Array was already created.");

  vao.create();
  context.bind_vao(vao);

  for (int i = 0; i < num_vbos; i++) {
    Vbo vbo;
    vbo.create();
    vbo.bind();
    gl::configure_vertex_attribute_pointers(vbo_descriptors[i]);
    vbos.push_back(std::move(vbo));
  }

  if (has_ebo) {
    ebo.create();
    ebo.bind();
  }

  context.unbind_vao(vao);
}

GROVE_NAMESPACE_END
