#version 450

layout (location = 0) out vec4 frag_color;

layout (location = 0) in vec4 v_proj_position;
layout (location = 1) in vec4 v_color;
layout (location = 2) in vec2 v_position11;
layout (location = 3) in vec3 v_normal;
layout (location = 4) in vec3 v_world_position;

layout (push_constant, std140) uniform PushConstantData {
  mat4 projection_view;
  vec4 camera_position;
  vec4 sun_position_strength;
};

float light_strength() {
  vec3 h = normalize(normalize(sun_position_strength.xyz) + normalize(camera_position.xyz - v_world_position));
  float spec_strength = pow(max(dot(h, normalize(v_normal)), 0.0), 4.0);
  return spec_strength * sun_position_strength.w;
}

void main() {
  if (length(v_position11) > 1.0) {
    discard;
  }

#if 1
  float ls = mix(0.0, 0.125, light_strength());
#else
  float ls = 1.0;
#endif
  frag_color = vec4(ls + v_color.rgb, 1.0);
}
