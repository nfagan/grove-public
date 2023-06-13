#version 450 core

#define ENERGY_CONSERVATION (0.95)

//  @NOTE: Value must match the value in `gi-sample-probes.glsl`
#define MAJERCIK_ET_AL_BORDER_HANDLING (0)

layout (location = 0) out vec4 irradiance;

layout (set = 0, binding = 0) uniform sampler2D ray_direction_texture;
layout (set = 0, binding = 1) uniform sampler2D ray_radiance_texture;
layout (set = 0, binding = 2) uniform sampler2D probe_irradiance_texture;

#ifdef USE_PER_PROBE_HYST
layout (set = 0, binding = 3) uniform usamplerBuffer per_probe_hyst;
#endif

layout (std140, push_constant) uniform PushConstantData {
  int ray_texture_dim;
  int rays_per_probe;
  int total_num_probes;

  int per_probe_inner_texture_dim;
  int per_probe_with_border_texture_dim;
  int per_probe_with_pad_texture_dim;
  int probes_per_array_texture_dim;

  float hyst;
  int output_texel_direction;
  int check_finite;
};

#pragma include "gi/oct.glsl"

ivec2 handle_border(ivec2 cell_rel, int assign_other) {
  cell_rel.x = (per_probe_with_border_texture_dim - 1) - cell_rel.x;
  cell_rel.y = assign_other;
  return cell_rel;
}

ivec2 lower_border(ivec2 cell_rel) {
  if (cell_rel.x == 0) {
    //  (0, 0) | D
    cell_rel.x = per_probe_inner_texture_dim;
    cell_rel.y = per_probe_inner_texture_dim;
  } else if (cell_rel.x == per_probe_with_border_texture_dim-1) {
    //  (1, 0) | C
    cell_rel.x = 1;
    cell_rel.y = per_probe_inner_texture_dim;
  } else {
    cell_rel = handle_border(cell_rel, 1);
  }
  return cell_rel;
}

#if MAJERCIK_ET_AL_BORDER_HANDLING
ivec2 upper_border(ivec2 cell_rel) {
  if (cell_rel.x == 0) {
    //  (1, 0) | B
    cell_rel.x = per_probe_inner_texture_dim;
    cell_rel.y = 1;
  } else if (cell_rel.x == per_probe_with_border_texture_dim-1) {
    //  (1, 1) | A
    cell_rel.x = 1;
    cell_rel.y = 1;
  } else {
    cell_rel = handle_border(cell_rel, per_probe_inner_texture_dim);
  }
  return cell_rel;
}
#endif

ivec2 remap_pixel(ivec2 cell_rel) {
  //  @TODO: I believe my probe uv's are subtly wrong, but I can't pinpoint the error.
  //  In "Scaling Probe-Based Real-Time Dynamic Global Illumination for Production" (2020), they implement a separate
  //  compute pass for copying probe border texels. With MAJERCIK_ET_AL_BORDER_HANDLING == 1, the following remapping
  //  should be equivalent as far as I can tell. However, I'm getting obvious discontinuities when sampling and
  //  visualizing probes using this approach.
#if MAJERCIK_ET_AL_BORDER_HANDLING
  if (cell_rel.y == 0) {
    cell_rel = lower_border(cell_rel);
  } else if (cell_rel.x == 0) {
    cell_rel.yx = lower_border(cell_rel.yx);
  } else if (cell_rel.y == per_probe_with_border_texture_dim-1) {
    cell_rel = upper_border(cell_rel);
  } else if (cell_rel.x == per_probe_with_border_texture_dim-1) {
    cell_rel.yx = upper_border(cell_rel.yx);
  }
#else
  if (cell_rel.y == 0) {
    cell_rel = lower_border(cell_rel);
  } else if (cell_rel.x == 0) {
    cell_rel.yx = lower_border(cell_rel.yx);
  }
#endif
  return cell_rel;
}

void main() {
  ivec2 frag_coord = ivec2(gl_FragCoord.xy);
  vec3 curr_irradiance = texelFetch(probe_irradiance_texture, frag_coord, 0).rgb;

  int last_probe_end = probes_per_array_texture_dim * per_probe_with_pad_texture_dim;
  float ib_mask = float(frag_coord.x < last_probe_end) * float(frag_coord.y < last_probe_end);

  ivec2 probe_cell_index = min(ivec2(probes_per_array_texture_dim-1), frag_coord / per_probe_with_pad_texture_dim);
  ivec2 cell_rel = frag_coord - probe_cell_index * per_probe_with_pad_texture_dim;
  ib_mask *= float(cell_rel.x < per_probe_with_border_texture_dim) * float(cell_rel.y < per_probe_with_border_texture_dim);
  cell_rel = min(cell_rel, ivec2(per_probe_with_border_texture_dim-1));
  cell_rel = remap_pixel(cell_rel);

  ivec2 inner_cell_index = cell_rel - 1;
#if MAJERCIK_ET_AL_BORDER_HANDLING
  vec2 frac_inner_cell = (vec2(inner_cell_index) + 0.5) / float(per_probe_inner_texture_dim);
  vec2 probe_uv11 = frac_inner_cell * 2.0 - 1.0;
#else
  vec2 frac_inner_cell = vec2(inner_cell_index) / float(per_probe_inner_texture_dim);
//  float half_cell_span = 0.5 / float(per_probe_inner_texture_dim);
  float half_cell_span = 0.0;
  vec2 probe_uv11 = (frac_inner_cell + half_cell_span) * 2.0 - 1.0;
#endif
  vec3 texel_direction = oct_decode(probe_uv11);

  int linear_probe_index = probe_cell_index.x * probes_per_array_texture_dim + probe_cell_index.y;
  //  i-th texel in `ray_radiance_texture`
  int linear_ray_index = linear_probe_index * rays_per_probe;

#ifdef USE_PER_PROBE_HYST
  uint hyst_sample = texelFetch(per_probe_hyst, clamp(linear_probe_index, 0, total_num_probes-1)).r;
#else
  const uint hyst_sample = 0;
#endif
  float hyst_use = max(hyst, min(1.0, float(hyst_sample) / 255.0));

  vec3 sum_irradiance = vec3(0.0);
  float sum_weight = 0.0;
  const float epsilon = 1e-6;
  bool any_non_converged = false;

  for (int i = 0; i < rays_per_probe; i++) {
    int ray_index = linear_ray_index + i;
    int ray_col = ray_index / ray_texture_dim;
    int ray_row = ray_index - ray_col * ray_texture_dim;
    ivec2 ray_uv = ivec2(ray_col, ray_row);
    vec4 sum_ray_radiance = texelFetch(ray_radiance_texture, ray_uv, 0);
    vec4 ray_direction_info = texelFetch(ray_direction_texture, ray_uv, 0);
    if (sum_ray_radiance.w == 1.0) {
      vec3 ray_radiance = sum_ray_radiance.xyz;
      //  Next ray direction is first bounce, so integrate radiance from this ray.
      vec3 ray_direction = ray_direction_info.xyz;
      float weight = max(0.0, dot(ray_direction, texel_direction));
      sum_irradiance += weight * ray_radiance * ENERGY_CONSERVATION;
      sum_weight += weight;
    } else {
      any_non_converged = true;
    }
  }

  if (any_non_converged) {
    sum_irradiance = curr_irradiance;

  } else if (sum_weight > epsilon) {
    sum_irradiance /= sum_weight;
  }

  sum_irradiance *= ib_mask;
  irradiance = vec4(mix(curr_irradiance, sum_irradiance, hyst_use), hyst_use - hyst);

  if (output_texel_direction == 1) {
//    vec2 texel_dir = (texel_direction.xy * 0.5 + 0.5) * ib_mask;
//    irradiance = vec4(texel_dir.x, texel_dir.y, 1.0, 1.0);
    irradiance = vec4(length(texel_direction * ib_mask), length(texel_direction * ib_mask), 1.0, 1.0);
//    irradiance = vec4(probe_frac.x, probe_frac.y, 0.0, 1.0);
  }

  if (check_finite == 1) {
    bool any_inf = any(isinf(irradiance));
    bool any_nan = any(isnan(irradiance));
    float finite_val = any_inf || any_nan ? 1.0 : 0.0;
    irradiance = vec4(finite_val, finite_val, finite_val, 1.0);
  }
}
