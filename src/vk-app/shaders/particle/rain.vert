#version 450

layout (location = 0) in vec2 position;
layout (location = 1) in vec4 data0;
layout (location = 2) in vec4 data1;

layout (location = 0) out float v_alpha;
layout (location = 1) out vec2 v_position11;
layout (location = 2) out float v_rand01;

#define GLOBAL_UNIFORM_SET (0)
#define GLOBAL_UNIFORM_BINDING (0)
#pragma include "particle/rain-data.glsl"

#pragma include "z_rotation.glsl"

void main() {
  vec3 translation = data0.xyz;
  float alpha = data0.w;
  float rand01 = data1.x;
  float xy_rot = data1.y;
  vec2 scale = particle_scale_alpha_scale.xy;

  mat3 inv_view = transpose(mat3(view));
  inv_view[1] = vec3(0.0, 1.0, 0.0);
  vec3 p3 = inv_view * z_rotation(xy_rot) * vec3(scale * position, 0.0);
  p3 += translation;

  v_alpha = alpha;
  v_position11 = position;
  v_rand01 = rand01;

  gl_Position = projection * view * vec4(p3, 1.0);
}
