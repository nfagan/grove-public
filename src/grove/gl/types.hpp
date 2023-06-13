#pragma once

#include "grove/visual/types.hpp"
#include <functional>

namespace grove {

struct DrawDescriptor;
using DrawFunction = std::function<void(const DrawDescriptor&)>;

namespace draw_functions {
  void arrays(const DrawDescriptor& descriptor);
  void instanced_arrays(const DrawDescriptor& descriptor);
  void elements(const DrawDescriptor& descriptor);
  void instanced_elements(const DrawDescriptor& descriptor);
}

namespace gl {
  unsigned int integral_type(IntegralType type);
  unsigned int draw_type(DrawType type);
  unsigned int buffer_type(BufferType type);
  unsigned int draw_mode(DrawMode mode);
  unsigned int shader_type(ShaderType type);
  unsigned int texture_format(TextureFormat format);

  int wrap_method(TextureWrapMethod method);
  int filter_method(TextureFilterMethod method);

  //  E.g., 1 -> GL_RED, 2 -> GL_RG,
  int unsized_color_texture_internal_format_from_components(int num_components);
  void configure_vertex_attribute_pointers(VertexBufferDescriptor& descriptor,
                                           size_t byte_offset = 0);
}

struct DrawDescriptor {
public:
  DrawDescriptor() :
    mode(DrawMode::Triangles),
    offset(0),
    count(0),
    instance_count(0),
    indices_type(IntegralType::UnsignedInt),
    draw_function(&draw_functions::arrays) {
    //
  }

  void draw() const {
    draw_function(*this);
  }

  static DrawDescriptor arrays(DrawMode mode, size_t count, int offset = 0) {
    return DrawDescriptor(mode, offset, count, 0,
                          IntegralType::UnsignedInt, &draw_functions::arrays);
  }

  static DrawDescriptor elements(DrawMode mode, size_t count, IntegralType indices_type) {
    return DrawDescriptor(mode, 0, count, 0, indices_type, &draw_functions::elements);
  }

  static DrawDescriptor instanced_arrays(DrawMode mode,
                                         size_t count,
                                         size_t instance_count,
                                         int offset = 0) {
    return DrawDescriptor(mode, offset, count, instance_count,
                          IntegralType::UnsignedInt, &draw_functions::instanced_arrays);
  }

  static DrawDescriptor instanced_elements(DrawMode mode,
                                           size_t count,
                                           size_t instance_count,
                                           IntegralType indices_type) {
    return DrawDescriptor(
      mode, 0, count, instance_count, indices_type, &draw_functions::instanced_elements);
  }

private:
  DrawDescriptor(DrawMode mode, int offset, size_t count, size_t instance_count,
                 IntegralType indices_type, DrawFunction draw_function) :
  mode(mode),
  offset(offset),
  count(count),
  instance_count(instance_count),
  indices_type(indices_type),
  draw_function(std::move(draw_function)) {
    //
  }

public:
  DrawMode mode;
  int offset;
  size_t count;
  size_t instance_count;
  IntegralType indices_type;
  DrawFunction draw_function;
};

}