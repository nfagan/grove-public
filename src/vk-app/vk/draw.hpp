#pragma once

#include "common.hpp"

namespace grove::vk {

struct DrawDescriptor {
  uint32_t num_vertices{};
  uint32_t num_instances{};
  uint32_t vertex_offset{};
  uint32_t instance_offset{};
};

struct DrawIndexedDescriptor {
  uint32_t num_indices{};
  uint32_t num_instances{};
  uint32_t index_offset{};
  int32_t vertex_offset{};
  uint32_t instance_offset{};
};

namespace cmd {

inline void draw(VkCommandBuffer cmd, const DrawDescriptor* desc) {
  vkCmdDraw(
    cmd,
    desc->num_vertices,
    desc->num_instances,
    desc->vertex_offset,
    desc->instance_offset);
}

inline void draw_indexed(VkCommandBuffer cmd, const DrawIndexedDescriptor* desc) {
  vkCmdDrawIndexed(
    cmd,
    desc->num_indices,
    desc->num_instances,
    desc->index_offset,
    desc->vertex_offset,
    desc->instance_offset);
}

} //  cmd

}