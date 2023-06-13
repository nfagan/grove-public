#include "mesh.hpp"
#include "grove/common/common.hpp"
#include "grove/load/obj.hpp"

GROVE_NAMESPACE_BEGIN

VertexBufferDescriptor vertex_buffer_descriptor_from_obj_data(const obj::VertexData& vd) {
  VertexBufferDescriptor descriptor;
  for (int i = 0; i < int(vd.attribute_sizes.size()); i++) {
    const auto sz = vd.attribute_sizes[i];
    assert(sz > 0);
    descriptor.add_attribute(AttributeDescriptor::floatn(i, sz));
  }
  return descriptor;
}

GROVE_NAMESPACE_END
