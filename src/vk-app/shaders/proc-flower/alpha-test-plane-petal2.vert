#version 450

layout (location = 0) in vec2 position;

layout (location = 0) out vec2 v_uv;
layout (location = 1) out float v_texture_layer;
layout (location = 2) out vec3 v_light_space_position0;
layout (location = 3) out vec3 v_world_position;
layout (location = 4) flat out uvec4 v_color;

#pragma include "pi.glsl"
#pragma include "z_rotation.glsl"
#pragma include "frame.glsl"
#pragma include "wind.glsl"
#pragma include "proc-flower/wind.glsl"

struct AlphaTestPlaneInstance {
  float min_radius;
  float radius;
  float radius_power;
  float curl_scale;
  vec4 translation_direction_x;
  vec4 direction_y_texture_layer_leaf_y_frac_origin_xz;
  uvec4 colors;
};

layout (std430, set = 0, binding = 0) readonly buffer AlphaTestPlaneInstanceBuffer {
  AlphaTestPlaneInstance instances[];
};

layout (std140, set = 0, binding = 1) uniform GlobalData {
#pragma include "shadow/sample-struct-fields.glsl"
  mat4 view;
  mat4 sun_light_view_projection0;
  vec4 camera_position;
  vec4 sun_color;
};

layout (set = 0, binding = 2) uniform sampler2D wind_displacement_texture;

layout (push_constant) uniform PushContantData {
  mat4 projection_view;
  vec4 num_grid_points_xz_t_unused;
  vec4 wind_world_bound_xz;
};

vec2 sample_wind(vec2 world_xz) {
  return sample_wind_tip_displacement(world_xz, wind_world_bound_xz, wind_displacement_texture);
}

float decode_leaf_y_frac(uint a) {
  const uint mask = 0xffff;
  a = (a >> 16) & mask;
  return float(a) / float(mask);
}

float decode_texture_layer(uint a) {
  const uint mask = 0xffff;
  return float(a & mask);
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

float radius_function(float z01, float min_radius, float radius, float radius_power) {
  float r = radius;
  r *= pow(z01, radius_power);
  r = max(min_radius, r);
  return r;
}

float curl_function(float x11, float z01, float curl_scale) {
  return pow((max(0.0, z01 - 0.5) * 2.0), 4.0) * curl_scale;
}

vec2 normalized_shape(vec3 p, vec2 scale) {
  vec2 x_lims = x_limits(scale);
  vec2 z_lims = z_limits(scale);
  float x01 = clamp((p.x - x_lims.x) / (x_lims.y - x_lims.x), 0.0, 1.0);
  float z01 = clamp((p.z - z_lims.x) / (z_lims.y - z_lims.x), 0.0, 1.0);
  return vec2(x01, z01);
}

vec3 transform_shape(vec3 p, vec2 norm_p, in AlphaTestPlaneInstance instance, float pi, out vec2 uv) {
  float x01 = norm_p.x;
  float x11 = x01 * 2.0 - 1.0;
  float z01 = norm_p.y;

  float radius = radius_function(z01, instance.min_radius, instance.radius, instance.radius_power);
  p.z += curl_function(x11, z01, instance.curl_scale);

  float theta = p.x * pi;
  float st = sin(theta);
  float ct = cos(theta);

  uv = (radius / instance.radius * vec2(st, -ct)) * 0.5 + 0.5;
  p = vec3(st * radius, -ct * radius + p.y, p.z);
  //  permute
  return vec3(p.x, p.z, p.y);
}

void main() {
  AlphaTestPlaneInstance instance = instances[gl_InstanceIndex];

  vec3 leaf_position = instance.translation_direction_x.xyz;
  vec2 leaf_direction = vec2(instance.translation_direction_x.w, instance.direction_y_texture_layer_leaf_y_frac_origin_xz.x);
  vec2 leaf_origin_xz = instance.direction_y_texture_layer_leaf_y_frac_origin_xz.zw;

  uint texture_layer_y_frac = floatBitsToUint(instance.direction_y_texture_layer_leaf_y_frac_origin_xz.y);
  float texture_layer = decode_texture_layer(texture_layer_y_frac);
  float leaf_y_frac = decode_leaf_y_frac(texture_layer_y_frac);

  vec2 num_grid_points_xz = num_grid_points_xz_t_unused.xy;
  float t = num_grid_points_xz_t_unused.z;

  vec2 sampled_wind = sample_wind(leaf_origin_xz);

  vec2 pxy = position;
  const vec2 petal_scale = vec2(1.0);

  vec3 p = identity_shape_function(pxy, num_grid_points_xz, petal_scale);
  vec2 norm_p = normalized_shape(p, petal_scale);

#if 1
  float norm_atten = sin(norm_p.y * 2.0 * PI * 2.0);
  float rand_phase = p.x * 1024.0;
  float z_off = norm_atten * length(sampled_wind) * pow(norm_p.y, 1.0) * cos(t * 10.0 + rand_phase) * 0.05;
  p.z += z_off;
#endif

  vec2 uv;
  p = transform_shape(p, norm_p, instance, PI, uv);
  p *= 0.5;

#if 1
  uv = (uv * 2.0 - 1.0) * 1.25;
  uv = uv * 0.5 + 0.5;
#endif

  vec3 v = spherical_to_cartesian(leaf_direction);
  mat3 tip_m = make_coordinate_system_y(v);
  p = tip_m * p + leaf_position;
  //  wind
  p.xz += wind_displacement(t, leaf_y_frac, sampled_wind, leaf_origin_xz);

//  vs_out.v_normalized_position = norm_p;
  v_uv = uv;
  v_texture_layer = texture_layer;
  v_color = instance.colors;
  v_world_position = p;
  v_light_space_position0 = (sun_light_view_projection0 * vec4(p, 1.0)).xyz;

  gl_Position = projection_view * vec4(p, 1.0);
}
