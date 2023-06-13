#pragma once

#include "types.hpp"
#include "VertexBufferArray.hpp"
#include "GLRenderContext.hpp"

namespace grove {

namespace obj {
  struct VertexData;
}

class DrawComponent {
public:
  void initialize(GLRenderContext& context, const DrawDescriptor& draw_descriptor,
                  VertexBufferDescriptor* descriptors, int num_descriptors,
                  const void** data, const size_t* num_bytes, bool has_ebo = false);

  void initialize(GLRenderContext& context, const obj::VertexData& vertex_data);

  void initialize(GLRenderContext& context, const DrawDescriptor& draw_descriptor,
                  VertexBufferDescriptor& descriptor,
                  const void* data, size_t num_bytes, bool has_ebo = false);

  void initialize(GLRenderContext& context, const DrawDescriptor& draw_descriptor,
                  VertexBufferDescriptor& descriptor,
                  const void* data, size_t num_bytes,
                  const void* indices, size_t num_bytes_indices);

  void initialize(GLRenderContext& context, const DrawDescriptor& draw_descriptor,
                  VertexBufferDescriptor* descriptors, int num_descriptors,
                  const void** data, const size_t* num_bytes_data,
                  const void* indices, size_t num_bytes_indices);

  template <typename T, typename U>
  void initialize(GLRenderContext& context, const DrawDescriptor& draw_descriptor,
                  VertexBufferDescriptor& buffer_descriptor,
                  const std::vector<T>& data,
                  const std::vector<U>& indices);

  void dispose();

  void bind_vao(GLRenderContext& context) const;
  void draw() const;

  bool is_valid() const;

public:
  VertexBufferArray vertex_array;
  DrawDescriptor draw_descriptor;
};

/*
 * Impl
 */

template <typename T, typename U>
void DrawComponent::initialize(GLRenderContext& context,
                               const DrawDescriptor& draw_descrip,
                               VertexBufferDescriptor& buffer_descriptor,
                               const std::vector<T>& data,
                               const std::vector<U>& indices) {

  initialize(context, draw_descrip, buffer_descriptor,
             data.data(), data.size() * sizeof(T),
             indices.data(), indices.size() * sizeof(U));
}

}