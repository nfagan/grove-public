#version 450

layout (location = 0) in vec2 a_position;
layout (location = 1) in vec2 a_translation;
layout (location = 2) in float a_frustum_grid_texel;
layout (location = 3) in float a_rotation;

#define GRASS_UNIFORM_SET (0)
#define GRASS_UNIFORM_BINDING (0)
#pragma include "grass/grass-data.glsl"

layout (location = 0) out VS_OUT vs_out;

#define FRUSTUM_GRID_SET (0)
#define FRUSTUM_GRID_BINDING (1)
#pragma include "grass/frustum-grid-data.glsl"

layout (set = 0, binding = 4) uniform sampler2D wind_displacement_texture;
layout (set = 0, binding = 6) uniform sampler2D height_map_texture;

mat3 make_scale_matrix(vec3 scl, float rotation) {
  float ct = cos(rotation);
  float st = sin(rotation);

  mat3 rot_mat = mat3(0.0);
  rot_mat[0][0] = ct;
  rot_mat[0][1] = 0.0;
  rot_mat[0][2] = -st;

  rot_mat[1][0] = 0.0;
  rot_mat[1][1] = 1.0;
  rot_mat[1][2] = 0.0;

  rot_mat[2][0] = st;
  rot_mat[2][1] = 0.0;
  rot_mat[2][2] = ct;

  mat3 scale_mat = mat3(0.0);
  scale_mat[0][0] = scl.x;
  scale_mat[1][1] = scl.y;
  scale_mat[2][2] = scl.z;

  return rot_mat * scale_mat;
}

vec2 calculate_height_map_uv(vec2 world_pos, float terrain_grid_scale) {
  return (world_pos + terrain_grid_scale * 0.5f) / terrain_grid_scale;
}

float compute_height(vec2 uv) {
  return texture(height_map_texture, uv).r;
}

float calculate_height_scale_mix_factor(vec2 scale_extents, float camera_space_z_extent) {
  float model_height_scale_z_extent = scale_extents.y - scale_extents.x;
  float mix_factor = (camera_space_z_extent - scale_extents.x) / model_height_scale_z_extent;
  mix_factor = clamp(mix_factor, 0.0, 1.0);
  return mix_factor;
}

float calculate_frac_frustum(vec2 world_to_camera_offset) {
  vec2 norm_off = normalize(world_to_camera_offset);
  float len_off = length(world_to_camera_offset);
  float extent = dot(norm_off, camera_front_xz.xy) * len_off / frustum_grid_dims.w;
  return clamp(extent, 0.0, 1.0);
}

vec2 calculate_world_to_camera_offset(vec2 world_translation) {
  vec2 pos = camera_position.xz + camera_front_xz.xy * frustum_grid_dims.z;
  return world_translation - pos;
}

vec2 calculate_world_translation(vec2 translation_info) {
  return translation_info * frustum_grid_dims.xy + frustum_grid_dims.xy * a_translation;
}

vec2 calculate_wind_tip_displacement(vec2 world_pos_xz,
                                     vec4 wind_world_bound_xz,
                                     sampler2D wind_displacement_texture) {
  vec2 world_p0 = wind_world_bound_xz.xy;
  vec2 world_p1 = wind_world_bound_xz.zw;
  vec2 p = (world_pos_xz - world_p0) / (world_p1 - world_p0);
  vec2 sampled = texture(wind_displacement_texture, p).rg;
  return sampled;
}

void main() {
  const float PI = 3.141592653589793;
  float t = time_info.x;
  float terrain_grid_scale = terrain_grid_scale_max_diffuse_max_specular.x;

  float x = a_position.x;
  float y = a_position.y;
  float y3 = pow(y, 3.0);
  vec2 true_uv = vec2((x + 1.0) * 0.5, y);

  float noise_amount = a_rotation / PI;

  //  Taper in the blade
  vec3 position = vec3(a_position, 0.0);
  float taper_amount = -(pow(y, blade_scale_taper_power.w + noise_amount * 0.5) * sign(x));
  position.x += taper_amount;

  vec4 translation_info = frustum_grid_info[int(a_frustum_grid_texel)].info;
  float alpha = clamp(translation_info.a, 0.0, 1.0);

  vec2 world_translation = calculate_world_translation(translation_info.xy);
  vec2 world_uv = calculate_height_map_uv(world_translation, terrain_grid_scale);

  /*
  transform info
  */
  float height = compute_height(world_uv);
  /*
  end transform info
  */

  float camera_dist = length(world_translation - camera_position.xz);
  float height_factor = clamp(exp(-pow(camera_dist * 0.003, 2.0)), 0.0, 1.0);

  float rotation = a_rotation;

  vec4 camera_space_translation = view * vec4(world_translation.x, 0.0, world_translation.y, 1.0);
  float camera_space_z_extent = camera_space_translation.z;

  vec2 world_to_camera_offset = calculate_world_to_camera_offset(world_translation);
  float frac_frustum = calculate_frac_frustum(world_to_camera_offset);
  vec3 use_scale = blade_scale_taper_power.xyz;
  use_scale = mix(blade_scale_taper_power.xyz, next_blade_scale.xyz, pow(smoothstep(0.0, 1.0, frac_frustum), 1.0));

  use_scale.y += (noise_amount - 0.5) * 1.0;
  use_scale.x += noise_amount * 0.1;

  position.z += (noise_amount - 0.5) * 2.0 * pow(y, 0.5);

  position = make_scale_matrix(use_scale, rotation) * position;
  position.xz += world_translation;
  position.y *= alpha * height_factor;

  /*
  wind
  */
  vec2 tip_displacement = calculate_wind_tip_displacement(
    world_translation, wind_world_bound_xz, wind_displacement_texture);
  position.xz += tip_displacement * y;

  vec2 camera_right_xz = normalize(vec2(view[0][0], view[2][0]));
  vec2 rand_displace = length(tip_displacement) * camera_right_xz * cos(t * (3.0 + noise_amount * 1.5)) * y;
  position.xz += rand_displace;
  /*
  end wind
  */

  position.y += height;

  vec4 world_position = vec4(position, 1.0);
  vec3 base_position = vec3(position.x, height, position.z);

  float height_mix = calculate_height_scale_mix_factor(extent_info.xy, camera_space_z_extent);
  float alpha_factor = mix(extent_info.z, extent_info.w, height_mix);

  vs_out.v_y = y;
  vs_out.color_uv = world_uv;
  vs_out.v_alpha = alpha_factor;
  vs_out.v_position = position;
  vs_out.light_space_position0 = (sun_light_view_projection0 * vec4(base_position, 1.0)).xyz;

  gl_Position = projection * view * world_position;
}