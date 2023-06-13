#pragma once

#include "grove/visual/types.hpp"

namespace grove {

namespace obj {
  struct VertexData;
}

VertexBufferDescriptor vertex_buffer_descriptor_from_obj_data(const obj::VertexData& vd);

}