#pragma once

#include "grove/visual/types.hpp"

namespace grove {

bool copy_buffer(const void* src,
                 const VertexBufferDescriptor& src_desc,
                 const int* src_attr_indices,
                 void* dst,
                 const VertexBufferDescriptor& dst_desc,
                 const int* dst_attr_indices,
                 int num_attrs_copy,
                 size_t num_elements);

bool copy_buffer(const void* src,
                 const VertexBufferDescriptor& src_desc,
                 const int* src_attr_indices,
                 void* dst,
                 const VertexBufferDescriptor& dst_desc,
                 int num_attrs_copy,
                 size_t num_elements);

}