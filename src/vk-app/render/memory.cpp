#include "memory.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Temporary.hpp"

GROVE_NAMESPACE_BEGIN

bool copy_buffer(const void* src,
                 const VertexBufferDescriptor& src_desc,
                 const int* src_attr_indices,
                 void* dst,
                 const VertexBufferDescriptor& dst_desc,
                 const int* dst_attr_indices,
                 int num_attrs_copy,
                 size_t num_elements) {
  Temporary<size_t, 32> src_offsets;
  Temporary<size_t, 32> dst_offsets;
  Temporary<size_t, 32> dst_sizes;

  size_t* src_attr_offs = src_offsets.require(num_attrs_copy);
  size_t* dst_attr_offs = dst_offsets.require(num_attrs_copy);
  size_t* dst_attr_sizes = dst_sizes.require(num_attrs_copy);

  const auto& dst_attrs = dst_desc.get_attributes();
  const int num_dst_attrs = dst_desc.count_attributes();
  const auto& src_attrs = src_desc.get_attributes();
  const int num_src_attrs = src_desc.count_attributes();

  for (int i = 0; i < num_attrs_copy; i++) {
    const int src_attr_ind = src_attr_indices[i];
    const int dst_attr_ind = dst_attr_indices ? dst_attr_indices[i] : i;
    if (dst_attr_ind >= num_dst_attrs || src_attr_ind >= num_src_attrs) {
      return false;
    }
    const auto& src_attr = src_attrs[src_attr_ind];
    const auto& dst_attr = dst_attrs[dst_attr_ind];
    if (src_attr.type != dst_attr.type || src_attr.size != dst_attr.size) {
      return false;
    }
    src_attr_offs[i] = src_desc.ith_attribute_offset_bytes(src_attr_ind);
    dst_attr_offs[i] = dst_desc.ith_attribute_offset_bytes(dst_attr_ind);
    dst_attr_sizes[i] = dst_attr.size_bytes();
  }

  const size_t src_stride = src_desc.attribute_stride_bytes();
  const size_t dst_stride = dst_desc.attribute_stride_bytes();

  for (size_t i = 0; i < num_elements; i++) {
    auto src_off = i * src_stride;
    auto dst_off = i * dst_stride;
    for (int j = 0; j < num_attrs_copy; j++) {
      auto* read = static_cast<const unsigned char*>(src) + src_off + src_attr_offs[j];
      auto* write = static_cast<unsigned char*>(dst) + dst_off + dst_attr_offs[j];
      memcpy(write, read, dst_attr_sizes[j]);
    }
  }

  return true;
}

bool copy_buffer(const void* src,
                 const VertexBufferDescriptor& src_desc,
                 const int* src_attr_indices,
                 void* dst,
                 const VertexBufferDescriptor& dst_desc,
                 int num_attrs_copy,
                 size_t num_elements) {
  return copy_buffer(src,
                     src_desc,
                     src_attr_indices,
                     dst,
                     dst_desc,
                     nullptr,
                     num_attrs_copy,
                     num_elements);
}

GROVE_NAMESPACE_END
