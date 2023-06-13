#include "types.hpp"
#include "grove/visual/types.hpp"
#include "grove/common/logging.hpp"
#include "grove/common/common.hpp"
#include <glad/glad.h>
#include <algorithm>
#include <cassert>

GROVE_NAMESPACE_BEGIN

namespace {

bool use_integer_vertex_attribute_pointer(IntegralType type) {
  switch (type) {
    case IntegralType::UnconvertedUnsignedInt:
      return true;
    default:
      return false;
  }
}

} //  anon

void draw_functions::arrays(const DrawDescriptor& descriptor) {
  glDrawArrays(gl::draw_mode(descriptor.mode), descriptor.offset, (GLsizei)descriptor.count);
}

void draw_functions::instanced_arrays(const DrawDescriptor& descriptor) {
  glDrawArraysInstanced(gl::draw_mode(descriptor.mode), descriptor.offset,
    (GLsizei)descriptor.count, (GLsizei)descriptor.instance_count);
}

void draw_functions::elements(const DrawDescriptor& descriptor) {
  const auto draw_mode = gl::draw_mode(descriptor.mode);
  const auto indices_type = gl::integral_type(descriptor.indices_type);
  glDrawElements(draw_mode, (GLsizei)descriptor.count, indices_type, nullptr);
}

void draw_functions::instanced_elements(const DrawDescriptor& descriptor) {
  const auto draw_mode = gl::draw_mode(descriptor.mode);
  const auto indices_type = gl::integral_type(descriptor.indices_type);
  glDrawElementsInstanced(draw_mode, (GLsizei)descriptor.count, indices_type,
    nullptr, (GLsizei)descriptor.instance_count);
}

int gl::unsized_color_texture_internal_format_from_components(int num_components) {
  switch (num_components) {
    case 1:
      return GL_RED;
    case 2:
      return GL_RG;
    case 3:
      return GL_RGB;
    case 4:
      return GL_RGBA;
    default:
      assert(false);
      return 1;
  }
}

unsigned int gl::texture_format(TextureFormat format) {
  switch (format) {
    case TextureFormat::A:
      return GL_ALPHA;
    case TextureFormat::R:
      return GL_RED;
    case TextureFormat::RG:
      return GL_RG;
    case TextureFormat::RGB:
      return GL_RGB;
    case TextureFormat::RGBA:
      return GL_RGBA;
    case TextureFormat::RGBA_32F:
      return GL_RGBA32F;
    case TextureFormat::Depth:
      return GL_DEPTH_COMPONENT;
    default:
      assert(false);
      return GL_RED;
  }
}

int gl::filter_method(TextureFilterMethod method) {
  switch (method) {
    case TextureFilterMethod::Nearest:
      return GL_NEAREST;
    case TextureFilterMethod::Linear:
      return GL_LINEAR;
    case TextureFilterMethod::LinearMipmapLinear:
      return GL_LINEAR_MIPMAP_LINEAR;
    case TextureFilterMethod::None:
      return GL_LINEAR;
    default:
      assert(false);
      return GL_NEAREST;
  }
}

int gl::wrap_method(TextureWrapMethod method) {
  switch (method) {
    case TextureWrapMethod::Repeat:
      return GL_REPEAT;
    case TextureWrapMethod::MirroredRepeat:
      return GL_MIRRORED_REPEAT;
    case TextureWrapMethod::EdgeClamp:
      return GL_CLAMP_TO_EDGE;
    case TextureWrapMethod::BorderClamp:
      return GL_CLAMP_TO_BORDER;
    case TextureWrapMethod::None:
      return GL_CLAMP_TO_EDGE;
    default:
      assert(false);
      return GL_REPEAT;
  }
}

unsigned int grove::gl::shader_type(grove::ShaderType type) {
  switch (type) {
    case ShaderType::Vertex:
      return GL_VERTEX_SHADER;
    case ShaderType::Fragment:
      return GL_FRAGMENT_SHADER;
    case ShaderType::Compute:
      return GL_COMPUTE_SHADER;
    default:
      assert(false);
      return GL_VERTEX_SHADER;
  }
}

unsigned int gl::draw_type(grove::DrawType type) {
  switch (type) {
    case DrawType::Static:
      return GL_STATIC_DRAW;
    case DrawType::Dynamic:
      return GL_DYNAMIC_DRAW;
    default:
      assert(false);
      return GL_STATIC_DRAW;
  }
}

unsigned int gl::buffer_type(BufferType type) {
  switch (type) {
    case BufferType::Array:
      return GL_ARRAY_BUFFER;
    case BufferType::Element:
      return GL_ELEMENT_ARRAY_BUFFER;
    case BufferType::Texture:
      return GL_TEXTURE_BUFFER;
    case BufferType::ShaderStorage:
      return GL_SHADER_STORAGE_BUFFER;
    case BufferType::DrawIndirect:
      return GL_DRAW_INDIRECT_BUFFER;
    default:
      assert(false);
      return GL_ARRAY_BUFFER;
  }
}

unsigned int gl::draw_mode(DrawMode mode) {
  switch (mode) {
    case DrawMode::Triangles:
      return GL_TRIANGLES;
    case DrawMode::TriangleStrip:
      return GL_TRIANGLE_STRIP;
    case DrawMode::Lines:
      return GL_LINES;
    case DrawMode::Points:
      return GL_POINTS;
    default:
      assert(false);
      return GL_TRIANGLES;
  }
}

unsigned int gl::integral_type(IntegralType type) {
  switch (type) {
    case IntegralType::Byte:
      return GL_BYTE;
    case IntegralType::UnsignedByte:
      return GL_UNSIGNED_BYTE;
    case IntegralType::Short:
      return GL_SHORT;
    case IntegralType::UnsignedShort:
      return GL_UNSIGNED_SHORT;
    case IntegralType::Int:
      return GL_INT;
    case IntegralType::UnsignedInt:
    case IntegralType::UnconvertedUnsignedInt:
      return GL_UNSIGNED_INT;
    case IntegralType::HalfFloat:
      return GL_HALF_FLOAT;
    case IntegralType::Float:
      return GL_FLOAT;
    case IntegralType::Double:
      return GL_DOUBLE;
    default:
      assert(false);
      return GL_BYTE;
  }
}

void gl::configure_vertex_attribute_pointers(VertexBufferDescriptor& descriptor,
                                             size_t byte_offset) {
  descriptor.sort_attributes_by_location();
  const size_t attribute_stride = descriptor.attribute_stride_bytes();

  for (auto& attribute_descriptor : descriptor) {
    const int location = attribute_descriptor.location;
    const int size = attribute_descriptor.size;

    const GLenum type = gl::integral_type(attribute_descriptor.type);
    const GLboolean normalize = attribute_descriptor.normalize;

    if (use_integer_vertex_attribute_pointer(attribute_descriptor.type)) {
      glVertexAttribIPointer(
        location, size, type, (GLsizei) attribute_stride, (void*) byte_offset);
    } else {
      glVertexAttribPointer(
        location, size, type, normalize, (GLsizei) attribute_stride, (void*) byte_offset);
    }

    glEnableVertexAttribArray(location);

    if (attribute_descriptor.divisor >= 0) {
      glVertexAttribDivisor(location, attribute_descriptor.divisor);
    }

    byte_offset += size_of_integral_type(attribute_descriptor.type) * size;
  }
}

GROVE_NAMESPACE_END