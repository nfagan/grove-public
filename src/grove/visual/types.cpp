#include "types.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Optional.hpp"
#include <cassert>
#include <algorithm>

GROVE_NAMESPACE_BEGIN

TextureFormat from_num_components(int num_components) {
  switch (num_components) {
    case 1:
      return TextureFormat::R;
    case 2:
      return TextureFormat::RG;
    case 3:
      return TextureFormat::RGB;
    case 4:
      return TextureFormat::RGBA;
    default:
      assert(false);
      return TextureFormat::RGBA;
  }
}

std::size_t size_of_integral_type(IntegralType type) {
  switch (type) {
    case IntegralType::Byte:
      return 1;
    case IntegralType::UnsignedByte:
      return 1;
    case IntegralType::Short:
      return 2;
    case IntegralType::UnsignedShort:
      return 2;
    case IntegralType::Int:
      return 4;
    case IntegralType::UnsignedInt:
    case IntegralType::UnconvertedUnsignedInt:
      return 4;
    case IntegralType::HalfFloat:
      return 2;
    case IntegralType::Float:
      return 4;
    case IntegralType::Double:
      return 8;
    default:
      assert(false);
      return 1;
  }
}

Optional<IntegralType> image::Channels::single_channel_type() const {
  if (num_channels == 0) {
    return NullOpt{};
  }
  for (int i = 1; i < num_channels; i++) {
    if (channels[i] != channels[0]) {
      return NullOpt{};
    }
  }
  return Optional<IntegralType>(channels[0]);
}

bool image::Channels::has_single_channel_type(IntegralType type) const {
  if (auto maybe_type = single_channel_type()) {
    return maybe_type.value() == type;
  } else {
    return false;
  }
}

bool image::Channels::is_uint8n(int n) const {
  if (num_channels != n) {
    return false;
  }
  for (int i = 0; i < num_channels; i++) {
    if (channels[i] != IntegralType::UnsignedByte) {
      return false;
    }
  }
  return true;
}

bool image::Channels::is_floatn(int n) const {
  if (num_channels != n) {
    return false;
  }
  for (int i = 0; i < num_channels; i++) {
    if (channels[i] != IntegralType::Float) {
      return false;
    }
  }
  return true;
}

void VertexBufferDescriptor::add_attribute(const AttributeDescriptor& attr) {
  if (attr.is_valid() && num_attributes < capacity()) {
    attributes[num_attributes++] = attr;
  } else {
    assert(false);
  }
}

size_t VertexBufferDescriptor::attribute_stride_bytes() const {
  size_t byte_count{};
  for (int i = 0; i < num_attributes; i++) {
    byte_count += size_of_integral_type(attributes[i].type) * attributes[i].size;
  }
  return byte_count;
}

size_t VertexBufferDescriptor::ith_attribute_offset_bytes(int index) const {
  size_t off{};
  for (int i = 0; i < index; i++) {
    off += size_of_integral_type(attributes[i].type) * attributes[i].size;
  }
  return off;
}

int VertexBufferDescriptor::num_components_per_vertex() const {
  int num_components = 0;
  for (int i = 0; i < num_attributes; i++) {
    num_components += attributes[i].size;
  }
  return num_components;
}

void VertexBufferDescriptor::offset_attribute_locations(int by_amount) {
  for (int i = 0; i < num_attributes; i++) {
    attributes[i].location += by_amount;
  }
}

void VertexBufferDescriptor::sort_attributes_by_location() {
  std::sort(attributes.begin(), attributes.begin() + num_attributes, [](auto&& a, auto&& b) {
    return a.location < b.location;
  });
}

GROVE_NAMESPACE_END