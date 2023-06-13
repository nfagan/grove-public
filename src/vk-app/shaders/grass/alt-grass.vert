#version 450

#define UNIFORM_SET (0)
#define UNIFORM_BINDING (0)
#pragma include "grass/alt-grass-data.glsl"

#define FRUSTUM_GRID_SET (0)
#define FRUSTUM_GRID_BINDING (1)
#pragma include "grass/frustum-grid-data.glsl"

layout (set = 0, binding = 2) uniform sampler2D height_map_texture;
layout (set = 0, binding = 3) uniform sampler2D wind_displacement_texture;

layout (location = 0) in vec2 position;
layout (location = 1) in vec4 read_frustum_grid_info;

layout (location = 0) out VS_OUT vs_out;

#pragma include "pi.glsl"
#pragma include "wind.glsl"

vec3 sun_light_space_position0(vec3 base_position) {
  vec4 pos = sun_light_view_projection0 * vec4(base_position, 1.0);
  return vec3(pos);
}

float height_scale_mix_factor(vec2 scale_extents, float camera_space_z_extent) {
  float z_span = scale_extents.y - scale_extents.x;
  return clamp((camera_space_z_extent - scale_extents.x) / z_span, 0.0, 1.0);
}

float compute_sun_fade_out_frac(float camera_space_z_extent, vec2 sun_fade_out_extents) {
  float fade_out_span = sun_fade_out_extents.y - sun_fade_out_extents.x;
  return clamp((camera_space_z_extent - sun_fade_out_extents.x) / fade_out_span, 0.0, 1.0);
}

vec2 compute_world_uv(vec2 world_pos_xz, float terrain_grid_scale) {
  return (world_pos_xz + terrain_grid_scale * 0.5f) / terrain_grid_scale;
}

vec2 compute_transform_info(vec2 world_uv) {
#if 0
  //  @TODO
  return texture(height_map_texture, world_uv).rg;
#else
  float height = texture(height_map_texture, world_uv).r;
  float scale_adjust = 0.0;
  return vec2(height, scale_adjust);
#endif
}

void main() {
  vec2 frustum_grid_cell_size = frustum_grid_cell_size_terrain_grid_scale.xy;
  float terrain_grid_scale = frustum_grid_cell_size_terrain_grid_scale.z;
  vec2 near_height_scale_z_extents = near_scale_info.xy;
  vec2 near_height_scale_factors = near_scale_info.zw;
  vec2 sun_fade_out_extents = far_scale_info.xy;
  float t = time_max_diffuse_max_specular.x;

  vec2 cell_translation = read_frustum_grid_info.xy;
  int grid_texel = int(read_frustum_grid_info.z);
  float rand01 = read_frustum_grid_info.w * 2.0;
  float rand11 = rand01 * 2.0 - 1.0;

  vec4 translation_info = frustum_grid_info[grid_texel].info;
  vec2 world_translation = translation_info.xy * frustum_grid_cell_size + frustum_grid_cell_size * cell_translation;
  float active_mask = translation_info.a > 0.5 ? 1.0 : 0.0;

  //  transform info
  vec2 world_uv = compute_world_uv(world_translation, terrain_grid_scale);
  vec2 transform_info = compute_transform_info(world_uv);
  float height = transform_info.x;
  float scale_adjust = transform_info.y + 1.0;

  vec4 camera_space_translation = view * vec4(world_translation.x, 0.0, world_translation.y, 1.0);
  float camera_space_z_extent = camera_space_translation.z;

  vec3 p3 = vec3(position, 0.0);
  p3 *= active_mask;
  p3.y = p3.y * 0.5 + 0.5;
  float y01 = p3.y;

  vec3 scale = vec3(0.3, 2.0 + rand11 * 0.25, 1.0);
  //  adjust scale based on near-distance to camera.
  float near_frac = height_scale_mix_factor(near_height_scale_z_extents, camera_space_z_extent);
  float near_height_scale_mix_factor = 1.0 - (1.0 / exp(near_frac * 9.0));
#if 1
  float near_scale = mix(near_height_scale_factors.x, near_height_scale_factors.y, near_height_scale_mix_factor);
#else
  float near_scale = 1.0;
#endif
  scale.y *= near_scale;
  //  Apply scale adjustment.
  scale.xy *= scale_adjust;

  mat3 inv_view = transpose(mat3(view));
  p3 = inv_view * (scale * p3);
  vec3 world_position = p3 + vec3(world_translation.x, 0.0, world_translation.y);

  /*
  Wind
  */
  vec2 sampled_wind = sample_wind_tip_displacement(world_translation, wind_world_bound_xz, wind_displacement_texture);
  sampled_wind *= scale_adjust;

  vec2 cam_right_xz = normalize(inv_view[0].xz);
  world_position.xz += sampled_wind * y01;
  world_position.xz += length(sampled_wind) * cam_right_xz * cos(t * (3.0 + rand01 * 1.5)) * y01;
  /*
  End wind
  */
  //  height
  world_position.y += height;
  vec3 base_world_position = vec3(world_position.x, height, world_position.z);

  vs_out.v_normalized_position = position * 0.5 + 0.5;
  vs_out.v_rand = rand01;
  vs_out.v_scale = scale;
  vs_out.v_alpha = near_frac;
  vs_out.v_world_uv = world_uv;
  vs_out.v_world_pos = world_position;
  vs_out.v_sun_fade_out_frac = compute_sun_fade_out_frac(camera_space_z_extent, sun_fade_out_extents);
  vs_out.v_light_space_position0 = sun_light_space_position0(base_world_position);

  gl_Position = projection * view * vec4(world_position, 1.0);
}