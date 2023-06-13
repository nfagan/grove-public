#version 450
#extension GL_ARB_separate_shader_objects : enable

#ifndef UNIFORM_SET
#define UNIFORM_SET (1)
#endif
#define UNIFORM_BINDING (0)
#pragma include "static-model/simple-model-data.glsl"

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;

#ifndef DISABLE_VERTEX_OUTPUT
layout (location = 0) out VS_OUT vs_out;
#endif

void main() {
  vec4 world_pos = model * vec4(position, 1.0);

#ifndef DISABLE_VERTEX_OUTPUT
  vs_out.position = world_pos.xyz;
  vs_out.light_space_position0 = (sun_light_view_projection0 * world_pos).xyz;
  vs_out.normal = transpose(inverse(mat3(model))) * normal;
  vs_out.uv = uv;
#endif

  gl_Position = projection * view * world_pos;
}
