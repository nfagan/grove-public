#version 450

layout (location = 0) in vec2 position;
layout (location = 1) in uint instance_index;

layout (location = 0) out vec2 v_alpha_uv;
layout (location = 1) out vec2 v_color_uv;
layout (location = 2) out float v_texture_layer;
layout (location = 3) out vec3 v_light_space_position0;
layout (location = 4) out vec3 v_world_position;
layout (location = 5) flat out uvec4 v_color;

#pragma include "pi.glsl"
#pragma include "z_rotation.glsl"
#pragma include "y_rotation.glsl"
#pragma include "frame.glsl"
#pragma include "wind.glsl"
#pragma include "proc-flower/wind.glsl"
#pragma include "orn-foliage/small-instance-data.glsl"

struct ShaderInstance {
  vec3 translation;
  vec3 direction;
  float aspect;
  float scale;
  float tip_y_fraction;
  vec2 world_origin_xz;
  float texture_layer;
  float y_rotation_theta;
  uvec4 colors;
};

ShaderInstance to_shader_instance(in OrnamentalFoliageSmallInstanceData inst) {
  ShaderInstance result;
  result.translation = inst.translation_direction_x.xyz;
  result.direction = vec3(inst.translation_direction_x.w, inst.direction_yz_unused.xy);
  result.aspect = inst.min_radius_or_aspect;
  result.scale = inst.radius_or_scale;
  result.tip_y_fraction = inst.tip_y_fraction;
  result.world_origin_xz = vec2(inst.world_origin_x, inst.world_origin_z);
  result.texture_layer = float(inst.texture_layer_index);
  result.y_rotation_theta = inst.radius_power_or_y_rotation_theta;
  result.colors = uvec4(inst.color0, inst.color1, inst.color2, inst.color3);
  return result;
}

layout (std430, set = 0, binding = 0) readonly buffer InstanceBuffer {
  OrnamentalFoliageSmallInstanceData instances[];
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

vec3 normalized_shape(vec2 p, vec2 np) {
  float num_x = np.x;
  float num_y = np.y;
  float x_dim = floor(num_x * 0.5);
  float x = p.x / x_dim;
  float z = p.y / (num_y - 1.0);
  return vec3(x, 0.0, z * 2.0 - 1.0);
}

void main() {
  ShaderInstance instance = to_shader_instance(instances[instance_index]);

  vec3 leaf_position = instance.translation;
  vec2 leaf_origin_xz = instance.world_origin_xz;
  float texture_layer = instance.texture_layer;
  float leaf_y_frac = instance.tip_y_fraction;

  vec2 num_grid_points_xz = num_grid_points_xz_t_unused.xy;
  float t = num_grid_points_xz_t_unused.z;

  vec2 sampled_wind = sample_wind(leaf_origin_xz);

  vec2 pxy = position;
  vec3 petal_scale = vec3(instance.scale);

  vec3 p11 = normalized_shape(pxy, num_grid_points_xz);
  vec2 norm_p = p11.xz * 0.5 + 0.5;
  vec3 p = p11 * petal_scale * 0.5;
  p = y_rotation(instance.y_rotation_theta) * p;

#if 1
  float norm_atten = sin(norm_p.y * 2.0 * PI * 2.0);
  float rand_phase = p.x * 1024.0;
  float z_off = norm_atten * length(sampled_wind) * pow(norm_p.y, 1.0) * cos(t * 10.0 + rand_phase) * 0.05;
  p.z += z_off;
#endif

#if 1
  p.y += abs(p11.x) * 0.1;
#endif

  mat3 tip_m = make_coordinate_system_y(instance.direction);
  p = tip_m * p + leaf_position;
  //  wind
  p.xz += wind_displacement(t, leaf_y_frac, sampled_wind, leaf_origin_xz);

  v_alpha_uv = norm_p;
  v_color_uv = instance.translation.xz * 0.01;
  v_texture_layer = texture_layer;
  v_world_position = p;
  v_light_space_position0 = (sun_light_view_projection0 * vec4(p, 1.0)).xyz;
  v_color = instance.colors;

  gl_Position = projection_view * vec4(p, 1.0);
}
