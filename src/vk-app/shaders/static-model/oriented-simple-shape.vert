#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec4 color_orientation;
layout (location = 2) in vec4 scale_active;
layout (location = 3) in vec4 translation;

layout (push_constant) uniform PushConstantData {
  mat4 projection_view;
};

layout (location = 0) out vec4 v_color;
layout (location = 1) out float v_active;

#pragma include "pack/1u32_to_2fn.glsl"
#pragma include "pack/1u32_to_4fn.glsl"

vec4 unpack_color() {
  return pack_1u32_4fn(floatBitsToUint(color_orientation.x));
}

mat3 unpack_frame() {
  vec2 xc_xy = pack_1u32_2fn(floatBitsToUint(color_orientation.y));
  vec2 xc_z_yc_x = pack_1u32_2fn(floatBitsToUint(color_orientation.z));
  vec2 yc_yz = pack_1u32_2fn(floatBitsToUint(color_orientation.w));

  vec3 x = normalize(vec3(xc_xy, xc_z_yc_x.x) * 2.0 - 1.0);
  vec3 y = normalize(vec3(xc_z_yc_x.y, yc_yz) * 2.0 - 1.0);
  vec3 z = cross(x, y);
  return mat3(x, y, z);
}

void main() {
  mat3 m = unpack_frame();
  vec3 p = m * (scale_active.xyz * position) + translation.xyz;
  v_color = unpack_color();
  v_active = scale_active.w;
  gl_Position = projection_view * vec4(p, 1.0);
}