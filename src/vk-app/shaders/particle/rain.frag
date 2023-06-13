#version 450

layout (location = 0) out vec4 frag_color;

layout (location = 0) in float v_alpha;
layout (location = 1) in vec2 v_position11;
layout (location = 2) in float v_rand01;

#define GLOBAL_UNIFORM_SET (0)
#define GLOBAL_UNIFORM_BINDING (0)
#pragma include "particle/rain-data.glsl"

void main() {
  float len = length(v_position11);
  float alpha_scale = particle_scale_alpha_scale.z;
  vec3 color = vec3(1.0);

//  float alpha_taper = 1.0 - pow(len, 4.0);
  float alpha_taper = 1.0 - len;
  float alpha_lifetime = (1.0 - pow(v_alpha, 4.0));
//  float alpha_new_born = pow(clamp(v_alpha, 0.0, 0.2) * 5.0, 2.0);
  float alpha_new_born = 1.0;

#if 1
  float tot_alpha = alpha_lifetime * alpha_taper * alpha_new_born * alpha_scale;
#else
  float tot_alpha = 1.0;
#endif

  frag_color = vec4(color, tot_alpha);
}
