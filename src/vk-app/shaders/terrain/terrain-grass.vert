#version 450

layout (location = 0) in vec2 position;
layout (location = 1) in vec4 translation_rand01;
layout (location = 2) in vec4 up;

layout (location = 0) out vec2 v_normalized_position;
layout (location = 1) out vec3 v_position;
layout (location = 2) out vec3 v_position_ls;

layout (std140, push_constant) uniform PushConstantData {
  mat4 projection_view;
};

layout (set = 0, binding = 2) uniform sampler2D wind_image;

#pragma include "terrain/set0-uniform-buffer.glsl"

#pragma include "frame.glsl"
#pragma include "pi.glsl"
#pragma include "wind.glsl"

float get_time() {
  return sun_color_gb_time.z;
}

void main() {
  float rand01 = translation_rand01.w;
  float y01 = position.y * 0.5 + 0.5;
  float t = get_time();
  mat3 inv_view = transpose(mat3(view));

  vec3 p = vec3(position + vec2(0.0, 1.0), 0.0);

#if 0
  float wind_scale = 0.5;
  float y_scale = 1.0 + rand01 * 0.2;
  vec3 tot_scale = vec3(0.25);
  p = inv_view * (p * vec3(1.0, y_scale, 1.0) * tot_scale);
//  p = p * make_coordinate_system_y(up.xyz);
  p += translation_rand01.xyz;
#else
  float wind_scale = 0.1;
  float y_scale = 0.5;
  vec3 tot_scale = vec3(0.25);
  p = inv_view * (p * vec3(0.25, y_scale, 1.0) * tot_scale);
  //  p = p * make_coordinate_system_y(up.xyz);
  p += translation_rand01.xyz;
#endif

  vec2 sampled_wind = sample_wind_tip_displacement(p.xz, wind_world_bound_xz, wind_image);
  sampled_wind *= wind_scale;
  vec2 cam_right_xz = normalize(inv_view[0].xz);
  p.xz += sampled_wind * y01;
  p.xz += length(sampled_wind) * cam_right_xz * cos(t * (4.0 + rand01 * 1.5)) * y01;

  v_normalized_position = position;
  v_position = p;
  v_position_ls = (light_view_projection0 * vec4(p, 1.0)).xyz;

  gl_Position = projection_view * vec4(p, 1.0);
}
