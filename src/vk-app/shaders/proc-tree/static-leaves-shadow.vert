#version 450

layout (location = 0) in vec4 position_u;
layout (location = 1) in vec4 normal_v;
layout (location = 2) in vec4 instance_position_transform_buffer_index;
layout (location = 3) in vec2 instance_direction;
layout (location = 4) in uvec4 packed_axis_root_info0;
layout (location = 5) in uvec4 packed_axis_root_info1;
layout (location = 6) in uvec4 packed_axis_root_info2;

#ifdef ALPHA_TEST_ENABLED
layout (location = 0) out vec2 v_uv;
#endif

#pragma include "frame.glsl"

layout (push_constant) uniform PushConstantData {
  mat4 projection_view;
  vec4 model_scale_padded;
};

void main() {
  vec3 position = position_u.xyz;
  vec3 instance_pos = instance_position_transform_buffer_index.xyz;
  vec3 model_scale = model_scale_padded.xyz;

  mat3 m = make_coordinate_system_y(spherical_to_cartesian(instance_direction));
  vec3 pos = m * (position * model_scale) + instance_pos;

#ifdef ALPHA_TEST_ENABLED
  v_uv = vec2(position_u.w, normal_v.w);
#endif

  gl_Position = projection_view * vec4(pos, 1.0);
  gl_Position.z = gl_Position.z * 0.5 + 0.5;
}