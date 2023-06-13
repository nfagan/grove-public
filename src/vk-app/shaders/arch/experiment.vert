#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;

layout (location = 0) out vec3 v_color;
layout (location = 1) out vec3 v_position;
layout (location = 2) out vec3 v_normal;
layout (location = 3) out vec3 v_light_space_position0;
layout (location = 4) flat out float v_rand;

#pragma include "arch/experiment-data.glsl"

layout (push_constant) uniform PushConstantData {
  vec4 translation_scale;
  vec4 color;
};

void main() {
  vec4 world_pos = vec4(position * translation_scale.w + translation_scale.xyz, 1.0);
  v_normal = normal;
  v_color = color.xyz;
  v_position = world_pos.xyz;
  v_light_space_position0 = (sun_light_view_projection0 * world_pos).xyz;
  v_rand = clamp(sin((position.x + position.y + position.z) * 4096.0) * 0.5 + 0.5, 0.0, 1.0);
  gl_Position = projection * view * world_pos;
}
