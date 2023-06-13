#version 450

#define INCLUDE_DISPLACEMENT (0)

#define GLOBAL_UNIFORM_SET (0)
#define GLOBAL_UNIFORM_BINDING (0)

#define INSTANCE_UNIFORM_SET (1)
#define INSTANCE_UNIFORM_BINDING (0)

#pragma include "proc-flower/alpha-test-data.glsl"

layout (location = 0) in vec4 position_triangle_index_displace_texture_index;
layout (location = 1) in vec4 leaf_position_y_fraction;
layout (location = 2) in vec4 leaf_direction_origin_xz;
layout (location = 3) in vec4 radius_info;
layout (location = 4) in vec4 curl_info;
layout (location = 5) in vec4 transform_info;
layout (location = 6) in vec2 instance_info;

layout (location = 0) out VS_OUT vs_out;

#pragma include "pi.glsl"
#pragma include "z_rotation.glsl"
#pragma include "frame.glsl"
#pragma include "wind.glsl"
#pragma include "proc-flower/wind.glsl"

layout (set = GLOBAL_UNIFORM_SET, binding = 1) uniform sampler2D wind_displacement_texture;
//  Displacement info
#if INCLUDE_DISPLACEMENT
uniform samplerBuffer displacement_texture;
#endif

ShapeParams unpack_shape_params() {
  ShapeParams result;
  result.min_radius = radius_info.x;
  result.radius = radius_info.y;
  result.radius_power = radius_info.z;
  result.mix_texture_color = radius_info.w;
  //
  result.circumference_frac0 = curl_info.x;
  result.circumference_frac1 = curl_info.y;
  result.circumference_frac_power = curl_info.z;
  result.curl_scale = curl_info.w;
  //
  result.scale = transform_info.xy;
  result.min_z_discard_enabled = transform_info.z;
  //
  result.group_frac = instance_info.x;
  return result;
}

vec2 x_limits(vec2 scale) {
  return vec2(-scale.x, scale.x);
}

vec2 z_limits(vec2 scale) {
  return vec2(0.0, scale.y);
}

vec3 identity_shape_function(vec2 p, vec2 np, vec2 scale) {
  float num_x = np.x;
  float num_y = np.y;
  float x_dim = floor(num_x * 0.5);
  float x = p.x / x_dim;
  float z = p.y / (num_y - 1.0);
  return vec3(x * scale.x, 0.0, z * scale.y);
}

float radius_function(float z01, in ShapeParams params) {
  float r = params.radius;
  r *= pow(z01, params.radius_power);
  r = max(params.min_radius, r);
  return r;
}

float circumference_function(float z01, in ShapeParams params) {
  float e0 = params.circumference_frac0;
  float e1 = params.circumference_frac1;
  return mix(e0, e1, pow(z01, params.circumference_frac_power));
}

float curl_function(float x11, float z01, in ShapeParams params) {
  return pow((max(0.0, z01 - 0.5) * 2.0), 4.0) * params.curl_scale;
}

vec2 normalized_shape(vec3 p, vec2 scale) {
  vec2 x_lims = x_limits(scale);
  vec2 z_lims = z_limits(scale);
  float x01 = clamp((p.x - x_lims.x) / (x_lims.y - x_lims.x), 0.0, 1.0);
  float z01 = clamp((p.z - z_lims.x) / (z_lims.y - z_lims.x), 0.0, 1.0);
  return vec2(x01, z01);
}

vec3 transform_shape(vec3 p, vec2 norm_p, in ShapeParams params, float pi, out vec2 uv) {
  float x01 = norm_p.x;
  float x11 = x01 * 2.0 - 1.0;
  float z01 = norm_p.y;

  float radius = radius_function(z01, params);
  float circum_frac = circumference_function(z01, params);

  p.z += curl_function(x11, z01, params);

  float theta = p.x * pi * circum_frac;
  float st = sin(theta);
  float ct = cos(theta);

  uv = (radius / params.radius * vec2(st, -ct)) * 0.5 + 0.5;
  p = vec3(st * radius, -ct * radius + p.y, p.z);
  //  permute
  return vec3(p.x, p.z, p.y);
}

vec2 sample_wind(vec2 world_xz) {
  return sample_wind_tip_displacement(world_xz, wind_world_bound_xz, wind_displacement_texture);
}

void main() {
  ShapeParams shape_params = unpack_shape_params();

  vec3 leaf_position = leaf_position_y_fraction.xyz;
  float leaf_y_frac = leaf_position_y_fraction.w;
  vec2 leaf_direction = leaf_direction_origin_xz.xy;
  vec2 leaf_origin_xz = leaf_direction_origin_xz.zw;
  vec2 pxy = position_triangle_index_displace_texture_index.xy;
  float triangle_index = position_triangle_index_displace_texture_index.z;
  float displace_tex_index = position_triangle_index_displace_texture_index.w;

  float t = time_info.x;

  vec2 sampled_wind = sample_wind(leaf_origin_xz);

  vec3 p = identity_shape_function(pxy, num_grid_points_xz.xy, shape_params.scale);
  vec2 norm_p = normalized_shape(p, shape_params.scale);

#if 1
  float norm_atten = sin(norm_p.y * 2.0 * PI * 2.0);
  float rand_phase = p.x * 1024.0;
  float z_off = norm_atten * length(sampled_wind) * pow(norm_p.y, 1.0) * cos(t * 10.0 + rand_phase) * 0.05;
  p.z += z_off;
#endif

  vec2 uv;
  p = transform_shape(p, norm_p, shape_params, PI, uv);
  p *= group_scale.xyz;

  vec3 v = spherical_to_cartesian(leaf_direction);
  mat3 tip_m = make_coordinate_system_y(v);
  p = tip_m * p + leaf_position;
  //  wind
  p.xz += wind_displacement(t, leaf_y_frac, sampled_wind, leaf_origin_xz);

#if INCLUDE_DISPLACEMENT
  //  explode
  vec4 displace_info = texelFetch(displacement_texture, int(displace_tex_index));
  vec3 part_displace = displace_info.xyz;
  float part_displace_t = clamp(displace_info.w, 0.0, 1.0);

  float theta_dir = (sin(triangle_index * 1024.0) * 0.5 + 0.5) * PI * 2.0;
  vec3 wind_dir = spherical_to_cartesian(vec2(theta_dir, 0.0));

  p += part_displace;
  p += wind_dir * pow(part_displace_t, 0.1);
#else
  float part_displace_t = 0.0;
#endif

  vs_out.v_normalized_position = norm_p;
  vs_out.v_uv = uv;
  vs_out.v_min_z_discard_enabled = shape_params.min_z_discard_enabled;
  vs_out.v_mix_texture_color = shape_params.mix_texture_color;
  vs_out.v_displace_t = part_displace_t;
  vs_out.v_world_position = p;
  vs_out.v_light_space_position0 = (sun_light_view_projection0 * vec4(p, 1.0)).xyz;

  gl_Position = projection * view * vec4(p, 1.0);
}
