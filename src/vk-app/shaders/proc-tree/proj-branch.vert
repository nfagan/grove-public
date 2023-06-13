#version 450

layout (location = 0) in vec2 position;
layout (location = 1) in vec4 info0;
layout (location = 2) in vec4 info1;
layout (location = 3) in vec4 info2;
layout (location = 4) in vec4 info3;
layout (location = 5) in vec4 info4;

#pragma include "pi.glsl"
#pragma include "grid-geometry/cylinder.glsl"

layout (push_constant) uniform PushConstantData {
  mat4 projection_view;
  vec4 num_points_xz;
};

vec3 shape_function() {
  return cylinder_shape_function(num_points_xz.xy, position, PI);
}

void main() {
  vec3 dir_x = info0.xyz;
  vec3 cdir_x = vec3(info0.w, info1.xy);
  vec3 pos = vec3(info1.zw, info2.x);
  vec3 cpos = info2.yzw;
  vec3 dir_z = info3.xyz;
  float radius = info3.w;
  vec3 cdir_z = info4.xyz;
  float cradius = info4.w;

  dir_x = normalize(dir_x);
  dir_z = normalize(dir_z);
  vec3 dir_y = normalize(cross(dir_x, dir_z));
//  mat3 m = mat3(dir_x, dir_y, dir_z);
  mat3 m = mat3(dir_y, dir_z, dir_x);

  cdir_x = normalize(cdir_x);
  cdir_z = normalize(cdir_z);
  vec3 cdir_y = normalize(cross(cdir_x, cdir_z));
//  mat3 cm = mat3(cdir_x, cdir_y, cdir_z);
  mat3 cm = mat3(cdir_y, cdir_z, cdir_x);

  vec3 s = shape_function();
  float y = s.y;
  vec3 s0 = vec3(s.x, 0.0, s.z);

  vec3 p = m * (s0 * radius) + pos;
  vec3 cp = cm * (s0 * cradius) + cpos;
  float mix_t = y;
  vec3 world_pos = mix(p, cp, mix_t);

  gl_Position = projection_view * vec4(world_pos, 1.0);
}
