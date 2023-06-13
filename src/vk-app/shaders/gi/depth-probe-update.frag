#version 450 core

#define MAX_DISTANCE (10000.0)

layout (location = 0) out vec4 depth_info;

layout (set = 0, binding = 0) uniform sampler2D ray_direction_texture;
layout (set = 0, binding = 1) uniform sampler2D ray_depth_texture;
layout (set = 0, binding = 2) uniform sampler2D probe_depth_texture;

layout (std140, push_constant) uniform PushConstantData {
  int ray_texture_dim;
  int rays_per_probe;

  int per_probe_inner_texture_dim;
  int per_probe_with_border_texture_dim;
  int per_probe_with_pad_texture_dim;
  int probes_per_array_texture_dim;

  float max_probe_distance;
  float depth_weight_exponent;

  float hyst;
  int output_texel_direction;
  int check_finite;
  int pad[1];
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

void main() {
  ivec2 frag_coord = ivec2(gl_FragCoord.xy);
  vec2 curr_depth = texelFetch(probe_depth_texture, frag_coord, 0).rg;

  int last_probe_end = probes_per_array_texture_dim * per_probe_with_pad_texture_dim;
  float ib_mask = float(frag_coord.x < last_probe_end) * float(frag_coord.y < last_probe_end);

  ivec2 probe_cell_index = min(ivec2(probes_per_array_texture_dim-1), frag_coord / per_probe_with_pad_texture_dim);
  ivec2 cell_rel = frag_coord - probe_cell_index * per_probe_with_pad_texture_dim;
  ib_mask *= float(cell_rel.x < per_probe_with_border_texture_dim) * float(cell_rel.y < per_probe_with_border_texture_dim);
  cell_rel = min(cell_rel, ivec2(per_probe_with_border_texture_dim-1));

  //  check if we're on a border texel
  if (cell_rel.y == 0) {
    cell_rel = lower_border(cell_rel);
  } else if (cell_rel.x == 0) {
    cell_rel.yx = lower_border(cell_rel.yx);
  }

  ivec2 inner_cell_index = cell_rel - 1;
  vec2 frac_inner_cell = vec2(inner_cell_index) / float(per_probe_inner_texture_dim);
  //  float half_cell_span = 0.5 / float(per_probe_inner_texture_dim);
  float half_cell_span = 0.0;
  vec2 probe_uv11 = (frac_inner_cell + half_cell_span) * 2.0 - 1.0;
  vec3 texel_direction = oct_decode(probe_uv11);

  int linear_probe_index = probe_cell_index.x * probes_per_array_texture_dim + probe_cell_index.y;
  //  i-th texel in `ray_radiance_texture`
  int linear_ray_index = linear_probe_index * rays_per_probe;

  vec2 sum_depth = vec2(0.0);
  float sum_weight = 0.0;
  const float epsilon = 1e-6;
  bool any_non_converged = false;

  for (int i = 0; i < rays_per_probe; i++) {
    int ray_index = linear_ray_index + i;
    int ray_col = ray_index / ray_texture_dim;
    int ray_row = ray_index - ray_col * ray_texture_dim;
    ivec2 ray_uv = ivec2(ray_col, ray_row);
    vec3 ray_direction = normalize(texelFetch(ray_direction_texture, ray_uv, 0).rgb);
    vec4 ray_depth_info = texelFetch(ray_depth_texture, ray_uv, 0);
    float probe_dist = ray_depth_info.x;

    float weight = max(0.0, dot(ray_direction, texel_direction));
    weight = pow(weight, depth_weight_exponent);

    if (weight >= epsilon) {
      sum_depth += vec2(probe_dist * weight, (probe_dist * probe_dist) * weight);
      sum_weight += weight;
    }

    if (ray_depth_info.w != 1.0) {
      any_non_converged = true;
    }
  }

  if (sum_weight > epsilon) {
    sum_depth /= sum_weight;
  }

  sum_depth *= ib_mask;
  sum_depth = min(sum_depth, vec2(max_probe_distance, max_probe_distance * max_probe_distance));
  if (any_non_converged) {
    sum_depth = curr_depth;
  }
  depth_info = vec4(mix(curr_depth, sum_depth, hyst), 1.0, 1.0);

  if (output_texel_direction == 1) {
//    depth_info = vec4(length(texel_direction * ib_mask), length(texel_direction * ib_mask), 1.0, 1.0);
    depth_info = vec4(1.0);
  }

  if (check_finite == 1) {
    bool any_inf = any(isinf(depth_info));
    bool any_nan = any(isnan(depth_info));
    float finite_val = any_inf || any_nan ? 1.0 : 0.0;
    depth_info = vec4(finite_val, finite_val, finite_val, 1.0);
  }
}