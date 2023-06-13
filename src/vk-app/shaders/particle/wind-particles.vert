#version 450

layout (location = 0) in vec2 a_position;
layout (location = 1) in vec3 a_translation;
layout (location = 2) in vec3 a_rotation_alpha_scale;

layout (location = 0) out vec2 v_position;
layout (location = 1) out float v_alpha;

layout (push_constant) uniform PushConstants {
  mat4 projection_view;
};

mat3 make_rot_y(float theta) {
  float st = sin(theta);
  float ct = cos(theta);
  mat3 res = mat3(0.0);

  res[0][0] = ct;
  res[0][2] = st;
  res[2][0] = -st;
  res[2][2] = ct;
  res[1][1] = 1.0;

  return res;
}

void main() {
  mat3 rot = make_rot_y(a_rotation_alpha_scale.x);
#ifdef DISABLE_BLEND
  vec3 p = vec3(a_position.x, a_position.y, 0.0) * a_rotation_alpha_scale.z * a_rotation_alpha_scale.y;
#else
  vec3 p = vec3(a_position.x, a_position.y, 0.0) * a_rotation_alpha_scale.z;
#endif

  p = rot * p + a_translation;

  v_position = a_position;
#ifdef DISABLE_BLEND
  v_alpha = 1.0;
#else
  v_alpha = a_rotation_alpha_scale.y;
#endif
  gl_Position = projection_view * vec4(p, 1.0);
}
