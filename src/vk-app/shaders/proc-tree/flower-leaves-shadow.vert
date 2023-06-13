#version 450

#define GLOBAL_UNIFORM_SET (0)
#define GLOBAL_UNIFORM_BINDING (0)

#define INSTANCE_UNIFORM_SET (1)
#define INSTANCE_UNIFORM_BINDING (0)

#pragma include "proc-tree/flower-leaves-data-common.glsl"
#pragma include "proc-tree/flower-leaves-shadow-data.glsl"

layout (std140, set = INSTANCE_UNIFORM_SET, binding = 1) readonly buffer TransformBuffer {
  PetalTransformData transform_data[];
};

layout (location = 0) in vec4 position_petal_randomness_transform_index;
layout (location = 1) in vec4 translation_petal_fraction;
layout (location = 2) in vec4 instance_position_transform_buffer_index;
layout (location = 3) in vec2 instance_direction;
layout (location = 4) in uvec4 packed_axis_root_info0;
layout (location = 5) in uvec4 packed_axis_root_info1;
layout (location = 6) in uvec4 packed_axis_root_info2;

#pragma include "pi.glsl"
#pragma include "frame.glsl"
#pragma include "x_rotation.glsl"
#pragma include "y_rotation.glsl"
#pragma include "z_rotation.glsl"

vec2 x_limits() {
  return vec2(-petal_params.scale_x, petal_params.scale_x);
}

vec2 z_limits() {
  return vec2(0.0, petal_params.scale_y);
}

vec3 sin_shape_function(vec2 scale_xz) {
  float z = position_petal_randomness_transform_index.y / num_points.y;
  float x = position_petal_randomness_transform_index.x / floor(num_points.x * 0.5);
  float z_incr = 1.0 / num_points.y;
  float z_out = (1.0 - z) - z_incr;
  float use_x = sin(PI * z) * x * scale_xz.x;
  float use_z = z_out * scale_xz.y;
  return vec3(use_x, 0.0, use_z);
}

float radius_function(float z01, in PetalParameters params) {
  float r = params.radius + translation_petal_fraction.w * params.max_additional_radius;
  r = pow(z01, params.radius_power) * r;
  r = max(params.min_radius, r);
  return r;
}

float circumference_function(float z01, in PetalParameters params) {
  return mix(params.circum_frac0, params.circum_frac1, pow(z01, params.circum_frac_power));
}

vec3 transform_petal_point(vec3 p, in PetalParameters params, out vec2 relative_position) {
  vec2 min_max_xs = x_limits();
  float x11 = (p.x - min_max_xs.x) / (min_max_xs.y - min_max_xs.x) * 2.0 - 1.0;

  vec2 min_max_zs = z_limits();
  float z01 = (p.z - min_max_zs.x) / (min_max_zs.y - min_max_zs.x);

  relative_position = vec2(x11, z01);

  float radius = radius_function(z01, params);
  float circum_frac = circumference_function(z01, params);
  float base_x = radius * p.x;
  float base_y = radius;

  float theta = p.x * PI * circum_frac;
  p = vec3(sin(theta) * radius, -cos(theta) * radius + p.y, p.z);

  float petal_frac = translation_petal_fraction.w;
  float z_rot = 2.0 * PI * petal_frac;
  p = z_rotation(z_rot) * p;
  p = vec3(p.x, p.z, p.y);
  //  Lower based on petal fraction.
  p.y -= petal_params.max_negative_y_offset * petal_frac;
  return p;
}

void main() {
  vec3 model_scale = model_scale_shadow_scale.xyz;
  float shadow_scale = model_scale_shadow_scale.w;
  vec2 petal_params_scale = vec2(petal_params.scale_x, petal_params.scale_y);

  vec3 shape = sin_shape_function(petal_params_scale);
  vec2 relative_position = vec2(0.0);
  shape = transform_petal_point(shape, petal_params, relative_position);

  shape = x_rotation(PI * 0.5) * shape;
  shape = y_rotation(2.0 * PI * translation_petal_fraction.z) * shape;

  shape.y += translation_petal_fraction.y;
  mat3 m = make_coordinate_system_y(spherical_to_cartesian(instance_direction));

  vec3 pos = m * (shape * model_scale * shadow_scale) + instance_position_transform_buffer_index.xyz;
  //  petal transform
  int base_transform_buffer_index = int(instance_position_transform_buffer_index.w);
  int petal_transform_buffer_index = int(position_petal_randomness_transform_index.w);
  int transform_buffer_index = base_transform_buffer_index + petal_transform_buffer_index;
  vec3 displace_translation = transform_data[transform_buffer_index].translation.xyz;
  pos += displace_translation;

  vec4 trans_pos = light_view_projection * vec4(pos, 1.0);
  trans_pos.z = (trans_pos.z * 0.5 + 0.5);
  gl_Position = trans_pos;
}