#version 450

layout (location = 0) in vec2 position;
layout (location = 1) in uint instance_index;

layout (location = 0) out vec2 v_uv;
layout (location = 1) out float v_texture_layer;
layout (location = 2) out vec3 v_light_space_position0;
layout (location = 3) out vec3 v_world_position;
layout (location = 4) flat out uvec4 v_color;

#pragma include "pi.glsl"
#pragma include "x_rotation.glsl"
#pragma include "z_rotation.glsl"
#pragma include "frame.glsl"
#pragma include "wind.glsl"
#pragma include "proc-flower/wind.glsl"
#pragma include "orn-foliage/large-instance-data.glsl"
#pragma include "proc-tree/wind.glsl"
#pragma include "proc-tree/wind-displacement.glsl"

struct ShaderInstance {
  vec3 translation;
  vec3 direction;
  float min_radius;
  float radius;
  float radius_power;
  float curl_scale;
  float texture_layer;
  uint aggregate_index;
  uvec4 colors;
  uvec4 wind_info0;
  uvec4 wind_info1;
  uvec4 wind_info2;
};

struct ShaderInstanceAggregate {
  vec3 aabb_p0;
  vec3 aabb_p1;
};

ShaderInstanceAggregate to_shader_instance_aggregate(in OrnamentalFoliageLargeInstanceAggregateData aggregate) {
  ShaderInstanceAggregate result;
  result.aabb_p0 = aggregate.aggregate_aabb_p0.xyz;
  result.aabb_p1 = aggregate.aggregate_aabb_p1.xyz;
  return result;
}

ShaderInstance to_shader_instance(in OrnamentalFoliageLargeInstanceData inst) {
  ShaderInstance result;
  result.translation = inst.translation_direction_x.xyz;
  result.direction = vec3(inst.translation_direction_x.w, inst.direction_yz_unused.xy);
  result.min_radius = inst.min_radius_or_aspect;
  result.radius = inst.radius_or_scale;
  result.radius_power = inst.radius_power_or_y_rotation_theta;
  result.curl_scale = inst.curl_scale;
  result.texture_layer = float(inst.texture_layer_index);
  result.aggregate_index = inst.aggregate_index;
  result.colors = uvec4(inst.color0, inst.color1, inst.color2, inst.color3);
  result.wind_info0 = inst.wind_info0;
  result.wind_info1 = inst.wind_info1;
  result.wind_info2 = inst.wind_info2;
  return result;
}

layout (std430, set = 0, binding = 0) readonly buffer InstanceBuffer {
  OrnamentalFoliageLargeInstanceData instances[];
};

layout (std430, set = 0, binding = 1) readonly buffer AggregateBuffer {
  OrnamentalFoliageLargeInstanceAggregateData aggregates[];
};

layout (std140, set = 0, binding = 2) uniform GlobalData {
#pragma include "shadow/sample-struct-fields.glsl"
  mat4 view;
  mat4 sun_light_view_projection0;
  vec4 camera_position;
  vec4 sun_color;
};

layout (set = 0, binding = 3) uniform sampler2D wind_displacement_texture;

layout (push_constant) uniform PushContantData {
  mat4 projection_view;
  vec4 num_grid_points_xz_t_unused;
  vec4 wind_world_bound_xz;
  vec4 wind_displacement_info;
};

vec2 sample_wind(vec3 aabb_p0, vec3 aabb_p1, vec4 wind_world_bound_xz) {
  vec3 world_center = (aabb_p1 - aabb_p0) * 0.5 + aabb_p0;
  vec2 world_xz = world_center.xz;
  return sample_wind_tip_displacement(world_xz, wind_world_bound_xz, wind_displacement_texture);
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

vec3 transform_shape(vec3 p, vec2 norm_p, in ShaderInstance instance, float pi, out vec2 uv) {
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
  ShaderInstance instance = to_shader_instance(instances[instance_index]);
  ShaderInstanceAggregate aggregate = to_shader_instance_aggregate(aggregates[instance.aggregate_index]);

  vec3 leaf_position = instance.translation;
  float texture_layer = instance.texture_layer;

  vec2 num_grid_points_xz = num_grid_points_xz_t_unused.xy;
  float t = num_grid_points_xz_t_unused.z;

  //  wind sample
  vec2 wind_displacement_limits = wind_displacement_info.xy;
  vec2 wind_strength_limits = wind_displacement_info.zw;

  vec2 sampled_wind = sample_wind(aggregate.aabb_p0, aggregate.aabb_p1, wind_world_bound_xz);
  float wind_atten = wind_attenuation(sampled_wind, wind_displacement_limits, wind_strength_limits);
  //

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

  mat3 tip_m = make_coordinate_system_y(instance.direction);
  p = tip_m * p + leaf_position;
  //  wind
  vec4 root_info0;
  vec4 root_info1;
  vec4 root_info2;
  vec4 child_root_info0;
  vec4 child_root_info1;
  vec4 child_root_info2;
  unpack_axis_root_info(instance.wind_info0, root_info0, child_root_info0);
  unpack_axis_root_info(instance.wind_info1, root_info1, child_root_info1);
  unpack_axis_root_info(instance.wind_info2, root_info2, child_root_info2);

  vec3 trans = wind_displacement(leaf_position, t, aggregate.aabb_p0, aggregate.aabb_p1, root_info0, root_info1, root_info2, wind_atten);
  p += trans - leaf_position;

  //  vs_out.v_normalized_position = norm_p;
  v_uv = uv;
  v_texture_layer = texture_layer;
  v_color = instance.colors;
  v_world_position = p;
  v_light_space_position0 = (sun_light_view_projection0 * vec4(p, 1.0)).xyz;

  gl_Position = projection_view * vec4(p, 1.0);
}
