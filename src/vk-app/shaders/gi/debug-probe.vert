#version 450 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in float probe_index;

layout (set = 0, binding = 0) readonly buffer PositionIndices {
  ivec4 position_indices[];
};

#pragma include "gi/debug-probe-data.glsl"
#pragma include "gi/index.glsl"

layout (location = 0) out vec3 v_normal;
layout (location = 1) flat out float v_probe_index;

vec3 linear_probe_index_to_world_position(int linear_probe_index, vec3 grid_origin, vec3 grid_cell_size) {
  return vec3(position_indices[linear_probe_index].rgb) * grid_cell_size + grid_origin;
}

void main() {
  const float radius = 0.5;

  vec3 pos = linear_probe_index_to_world_position(int(probe_index), probe_grid_origin.xyz, probe_grid_cell_size.xyz);
  vec3 p = position;
  p *= radius;
  p += pos;

  v_normal = normal;
  v_probe_index = probe_index;

  gl_Position = projection_view * vec4(p, 1.0);
}
