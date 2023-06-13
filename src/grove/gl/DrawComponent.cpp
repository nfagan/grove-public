#include "DrawComponent.hpp"
#include "grove/load/obj.hpp"

namespace grove {

void DrawComponent::initialize(grove::GLRenderContext& context,
                               const grove::DrawDescriptor& draw_descr,
                               grove::VertexBufferDescriptor* descriptors, int num_descriptors,
                               const void** data, const size_t* num_bytes, bool has_ebo) {
  draw_descriptor = draw_descr;
  vertex_array.create(context, descriptors, num_descriptors, has_ebo);

  for (int i = 0; i < num_descriptors; i++) {
    vertex_array.vbos[i].bind_fill(data[i], num_bytes[i], descriptors[i].draw_type);
  }
}

void DrawComponent::initialize(GLRenderContext& context, const obj::VertexData& vertex_data) {
  VertexBufferDescriptor descriptor;
  int attrib_index = 0;

  for (const int size : vertex_data.attribute_sizes) {
    descriptor.add_attribute(AttributeDescriptor::floatn(attrib_index++, size));
  }

  auto tmp_draw_descriptor =
    DrawDescriptor::arrays(DrawMode::Triangles, vertex_data.num_vertices());

  initialize(context, tmp_draw_descriptor, descriptor,
    &vertex_data.packed_data[0], vertex_data.packed_data.size() * sizeof(float));
}

void DrawComponent::initialize(grove::GLRenderContext& context,
                               const grove::DrawDescriptor& draw_descr,
                               grove::VertexBufferDescriptor& descriptor,
                               const void* data, size_t num_bytes, bool has_ebo) {
  initialize(context, draw_descr, &descriptor, 1, &data, &num_bytes, has_ebo);
}

void DrawComponent::initialize(grove::GLRenderContext& context,
                               const grove::DrawDescriptor& draw_descr,
                               grove::VertexBufferDescriptor& descriptor,
                               const void* data, size_t num_bytes,
                               const void* indices, size_t num_bytes_indices) {

  initialize(context, draw_descr, descriptor, data, num_bytes, true);
  vertex_array.ebo.bind_fill(indices, num_bytes_indices);
}

void DrawComponent::initialize(grove::GLRenderContext& context,
                               const grove::DrawDescriptor& draw_descr,
                               grove::VertexBufferDescriptor* descriptors, int num_descriptors,
                               const void** data, const size_t* num_bytes_data,
                               const void* indices, size_t num_bytes_indices) {

  initialize(context, draw_descr, descriptors, num_descriptors, data, num_bytes_data, true);
  vertex_array.ebo.bind_fill(indices, num_bytes_indices);
}

void DrawComponent::bind_vao(grove::GLRenderContext& context) const {
  context.bind_vao(vertex_array.vao);
}

void DrawComponent::draw() const {
  draw_descriptor.draw();
}

bool DrawComponent::is_valid() const {
  return vertex_array.is_valid();
}

void DrawComponent::dispose() {
  vertex_array.dispose();
}

}
