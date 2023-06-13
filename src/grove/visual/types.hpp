#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <cassert>

namespace grove {

template <typename T>
class Optional;

enum class TextureFilterMethod {
  Nearest,
  Linear,
  LinearMipmapLinear,
  None
};

enum class TextureWrapMethod {
  Repeat,
  MirroredRepeat,
  EdgeClamp,
  BorderClamp,
  None
};

enum class TextureFormat {
  A,
  R,
  RG,
  RGB,
  RGBA,
  RGBA_32F,
  Depth
};

enum class DrawMode {
  Triangles,
  TriangleStrip,
  Lines,
  Points
};

enum class BufferType {
  Array,
  Element,
  Texture,
  ShaderStorage,
  DrawIndirect
};

enum class ShaderType {
  Vertex,
  Fragment,
  Compute
};

enum class DrawType {
  Static,
  Dynamic
};

enum class IntegralType {
  Byte,
  UnsignedByte,
  Short,
  UnsignedShort,
  Int,
  UnsignedInt,
  UnconvertedUnsignedInt,
  HalfFloat,
  Float,
  Double
};

enum class IntConversion {
  None,
  UNorm,
  SNorm,
  UScaled,
  SScaled,
};

namespace limits {
  static constexpr int max_num_attributes_per_vertex_buffer = 10;
  static constexpr int max_num_texture_stack_frames = 10;
  static constexpr int max_num_active_textures = 32;
}

std::size_t size_of_integral_type(IntegralType type);
TextureFormat from_num_components(int num_components);

namespace image {

struct Shape {
  static Shape make_2d(int width, int height) {
    return Shape{width, height, 1};
  }
  static Shape make_3d(int width, int height, int depth) {
    return Shape{width, height, depth};
  }
  int64_t num_elements() const {
    return int64_t(width) * int64_t(height) * int64_t(depth);
  }

  int width;
  int height;
  int depth;
};

struct Channels {
  static constexpr int max_num_channels = 8;
  using Array = std::array<IntegralType, max_num_channels>;

  static Channels make_n_of_type(int num, IntegralType type) {
    assert(num > 0);
    Channels res{};
    for (int i = 0; i < num; i++) {
      res.add_channel(type);
    }
    return res;
  }

  static Channels make_uint8n(int num) {
    return make_n_of_type(num, IntegralType::UnsignedByte);
  }

  static Channels make_floatn(int num) {
    return make_n_of_type(num, IntegralType::Float);
  }

  void add_channel(IntegralType type) {
    assert(num_channels < max_num_channels);
    channels[num_channels++] = type;
  }

  Optional<IntegralType> single_channel_type() const;
  bool has_single_channel_type(IntegralType type) const;
  bool is_uint8n(int n) const;
  bool is_floatn(int n) const;

  IntegralType operator[](int idx) const {
    return channels[idx];
  }

  IntegralType& operator[](int idx) {
    return channels[idx];
  }

  size_t size_bytes() const {
    size_t sz{};
    for (int i = 0; i < num_channels; i++) {
      sz += size_of_integral_type(channels[i]);
    }
    return sz;
  }

  Array channels;
  int num_channels;
};

struct Descriptor {
  static Descriptor make_2d_floatn(int w, int h, int nc) {
    return Descriptor{Shape::make_2d(w, h), Channels::make_floatn(nc)};
  }
  static Descriptor make_2d_uint8n(int w, int h, int nc) {
    return Descriptor{Shape::make_2d(w, h), Channels::make_uint8n(nc)};
  }
  static Descriptor make_2d_int32n(int w, int h, int nc) {
    return Descriptor{Shape::make_2d(w, h), Channels::make_n_of_type(nc, IntegralType::Int)};
  }

  void add_channel(IntegralType type) {
    channels.add_channel(type);
  }

  size_t element_size_bytes() const {
    return channels.size_bytes();
  }

  int64_t num_elements() const {
    return shape.num_elements();
  }

  size_t total_size_bytes() const {
    return num_elements() * element_size_bytes();
  }

  bool is_2d() const {
    return shape.depth <= 1;
  }

  int rows() const {
    return shape.height;
  }
  int height() const {
    return shape.height;
  }
  int cols() const {
    return shape.width;
  }
  int width() const {
    return shape.width;
  }
  int num_channels() const {
    return channels.num_channels;
  }

  Shape shape;
  Channels channels;
};

} //  image

struct AttributeDescriptor {
  IntegralType type{IntegralType::Float};
  int size{};
  int location{-1};
  int divisor{-1};
  bool normalize{false};

  bool is_valid() const {
    return location >= 0 && size >= 1 && size <= 4;
  }

  bool is_floatn(int n) const {
    return type == IntegralType::Float && size == n;
  }

  std::size_t size_bytes() const {
    return size * size_of_integral_type(type);
  }

  static AttributeDescriptor unconverted_unsigned_intn(int location, int size, int divisor = -1) {
    AttributeDescriptor descriptor;
    descriptor.type = IntegralType::UnconvertedUnsignedInt;
    descriptor.size = size;
    descriptor.location = location;
    descriptor.divisor = divisor;
    return descriptor;
  }

  static AttributeDescriptor floatn(int location, int size, int divisor = -1) {
    AttributeDescriptor descriptor;
    descriptor.type = IntegralType::Float;
    descriptor.size = size;
    descriptor.location = location;
    descriptor.divisor = divisor;
    return descriptor;
  }

  static AttributeDescriptor unsigned_byten(int location, int size, int divisor = -1) {
    AttributeDescriptor descriptor;
    descriptor.type = IntegralType::UnsignedByte;
    descriptor.size = size;
    descriptor.location = location;
    descriptor.divisor = divisor;
    return descriptor;
  }

  static AttributeDescriptor float4(int location, int divisor = -1) {
    return AttributeDescriptor::floatn(location, 4, divisor);
  }

  static AttributeDescriptor float3(int location, int divisor = -1) {
    return AttributeDescriptor::floatn(location, 3, divisor);
  }

  static AttributeDescriptor float2(int location, int divisor = -1) {
    return AttributeDescriptor::floatn(location, 2, divisor);
  }

  static AttributeDescriptor float1(int location, int divisor = -1) {
    return AttributeDescriptor::floatn(location, 1, divisor);
  }

  static AttributeDescriptor unsigned_byte3(int location, int divisor = -1) {
    return AttributeDescriptor::unsigned_byten(location, 3, divisor);
  }
};

class VertexBufferDescriptor {
public:
  using Attributes =
    std::array<AttributeDescriptor, limits::max_num_attributes_per_vertex_buffer>;

public:
  void add_attribute(const AttributeDescriptor& attr);
  void offset_attribute_locations(int by_amount);

  int num_components_per_vertex() const;
  int count_attributes() const {
    return num_attributes;
  }

  size_t attribute_stride_bytes() const;
  size_t vertex_size_bytes() const {
    return attribute_stride_bytes();
  }
  size_t ith_attribute_offset_bytes(int index) const;
  size_t num_vertices(size_t at_data_size) const {
    return at_data_size / vertex_size_bytes();
  }

  static constexpr int capacity() {
    return limits::max_num_attributes_per_vertex_buffer;
  }

  void sort_attributes_by_location();

  const AttributeDescriptor* begin() const {
    return attributes.data();
  }
  const AttributeDescriptor* end() const {
    return attributes.data() + num_attributes;
  }
  const Attributes& get_attributes() const {
    return attributes;
  }

public:
  DrawType draw_type{DrawType::Static};

private:
  int num_attributes{};
  Attributes attributes;
};

}
