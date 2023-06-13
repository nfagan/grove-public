#version 450

layout (location = 0) in vec2 position;
layout (location = 1) in vec4 translation_scale;
layout (location = 2) in uvec4 color_rotation;

layout (location = 0) out vec4 v_proj_position;
layout (location = 1) out vec4 v_color;
layout (location = 2) out vec2 v_position11;
layout (location = 3) out vec3 v_normal;
layout (location = 4) out vec3 v_world_position;

layout (push_constant, std140) uniform PushConstantData {
  mat4 projection_view;
  vec4 camera_position;
  vec4 sun_position_strength;
};

#pragma include "pack/1u32_to_4u8.glsl"
#pragma include "x_rotation.glsl"
#pragma include "y_rotation.glsl"

vec4 parse_color() {
  return vec4(pack_1u32_4u8(color_rotation.x)) / 255.0;
}

mat3 parse_rotation() {
  vec2 rot;
  rot.x = uintBitsToFloat(color_rotation.y);
  rot.y = uintBitsToFloat(color_rotation.z);
  return y_rotation(rot.y) * x_rotation(rot.x);
}

void main() {
  vec3 pos = vec3(position, 0.0);
  mat3 m = parse_rotation();

  pos *= vec3(translation_scale.w, translation_scale.w, 1.0);
  pos = m * pos;
  pos += translation_scale.xyz;

  vec4 proj_pos = projection_view * vec4(pos, 1.0);

  v_proj_position = proj_pos;
  v_color = parse_color();
  v_position11 = position;
  v_normal = m[2];
  v_world_position = pos;

  gl_Position = proj_pos;
}
