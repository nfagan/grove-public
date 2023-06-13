#define ENERGY_CONSERVATION (0.95)
#define LINEAR_BLENDING (0)
//  @NOTE See irradiance-probe-update.frag
#define MAJERCIK_ET_AL_BORDER_HANDLING (0)

vec2 probe_uv(ProbeTextureInfo info, int linear_probe_index, vec3 normal) {
  vec2 inner_cell_uv = oct_encode(normal) * 0.5 + 0.5;
#if MAJERCIK_ET_AL_BORDER_HANDLING
  inner_cell_uv -= 0.5 / float(info.per_probe_inner_texture_dim);
#endif
  int probe_col = linear_probe_index / info.probes_per_array_texture_dim;
  int probe_row = linear_probe_index - probe_col * info.probes_per_array_texture_dim;

  int probe_px_row = probe_row * info.per_probe_with_pad_texture_dim;
  int probe_px_col = probe_col * info.per_probe_with_pad_texture_dim;
  int pixel_border_size = info.per_probe_with_border_texture_dim - info.per_probe_inner_texture_dim;

  vec2 inner_cell_rel = float(info.per_probe_inner_texture_dim) * inner_cell_uv;
  vec2 padded_cell_rel = inner_cell_rel + float(pixel_border_size) * 0.5;  //  add gutter.
  vec2 aligned_rel = padded_cell_rel + vec2(float(probe_px_col), float(probe_px_row));
  vec2 uv = (aligned_rel + 0.5) / float(info.probe_array_texture_dim);
  return uv;
}

#ifndef IRRADIANCE_PROBE_ONLY
vec2 sample_depth_probe(sampler2D probe_depth_texture, ProbeTextureInfo tex_info,
                        ProbeGridInfo grid_info, ivec3 probe_grid_index, vec3 probe_to_surfel_n) {
  int linear_probe_index = probe_grid_index_to_linear_index(probe_grid_index, ivec3(grid_info.probe_counts));
  vec2 uv = probe_uv(tex_info, linear_probe_index, probe_to_surfel_n);
  return texture(probe_depth_texture, uv).rg;
}
#endif

vec3 sample_irradiance_probe(sampler2D probe_irradiance_texture, ProbeTextureInfo tex_info,
                             ProbeGridInfo grid_info, ivec3 probe_grid_index, vec3 probe_to_surfel_n) {
  int linear_probe_index = probe_grid_index_to_linear_index(probe_grid_index, ivec3(grid_info.probe_counts));
  vec2 uv = probe_uv(tex_info, linear_probe_index, probe_to_surfel_n);
  return texture(probe_irradiance_texture, uv).rgb;
}

float sample_irradiance_probe(sampler2D probe_irradiance_texture, ProbeTextureInfo tex_info,
                              ProbeGridInfo grid_info, ivec3 probe_grid_index, vec3 probe_to_surfel_n, out vec3 irr) {
  int linear_probe_index = probe_grid_index_to_linear_index(probe_grid_index, ivec3(grid_info.probe_counts));
  vec2 uv = probe_uv(tex_info, linear_probe_index, probe_to_surfel_n);
  vec4 texel = texture(probe_irradiance_texture, uv);
  irr = texel.rgb;
  float alive_weight = pow(clamp(1.0 - texel.a, 0.0, 1.0), 2.0);
  return alive_weight;
}

#ifndef IRRADIANCE_PROBE_ONLY
float compute_grid_taper(vec3 surface_position, vec3 camera_position, ProbeGridInfo grid_info) {
  vec3 grid_span = grid_info.probe_grid_cell_size * grid_info.probe_counts * 0.5;
  float max_len = max(max(grid_span.x, grid_span.y), grid_span.z);
  return 1.0 - pow(clamp(length(camera_position - surface_position) / max_len, 0.0, 1.0), 2.0);
}

//  Define the position indices buffer before including this file:
//  readonly buffer PositionIndices { ivec4 position_indices[] };
vec3 sample_probes(sampler2D probe_irradiance_texture, sampler2D probe_depth_texture,
                   ProbeTextureInfo irr_tex_info, ProbeTextureInfo depth_tex_info, ProbeGridInfo grid_info,
                   vec3 surface_position, vec3 surface_normal, ProbeSampleParams params) {
#ifdef USE_GRID_TAPER
  float taper_weight = compute_grid_taper(surface_position, params.camera_position, grid_info);
  if (taper_weight < 0.001) {
    return vec3(0.0);
  }
#endif

#ifdef USE_SAMPLE_OFFSET
#ifndef NUM_POISSON_SAMPLES
#error "Missing poisson samples"
#endif
  uint ind = uint(clamp(mod(gl_FragCoord.x * gl_FragCoord.y, 64.0), 0.0, 63.0));
  vec2 poiss_sample = POISSON_SAMPLES[ind] * 0.5;
//  vec2 poiss_sample = POISSON_SAMPLES[ind] * 0.125;
  surface_position.xz += poiss_sample;
#endif
  vec3 grid_ori_relative = max(vec3(0.0), surface_position - grid_info.probe_grid_origin);
  vec3 cell_relative = grid_ori_relative / grid_info.probe_grid_cell_size;
  vec3 cell_ind = floor(cell_relative);
  vec3 cell_frac = cell_relative - cell_ind;
  ivec3 cell_index = ivec3(cell_ind);

  vec3 tot_irradiance = vec3(0.0);
  float tot_weight = 0.0;

  vec3 w_o = normalize(params.camera_position - surface_position);

  for (int i = 0; i < 8; i++) {
    float probe_life_weight = 1.0;
    ivec3 probe_offset = ivec3(i, i >> 1, i >> 2) & ivec3(1);
    ivec3 probe_grid_index = clamp(cell_index + probe_offset, ivec3(0), ivec3(grid_info.probe_counts)-1);
    int linear_probe_index = probe_grid_index_to_linear_index(probe_grid_index, ivec3(grid_info.probe_counts));
    ivec3 probe_index = position_indices[linear_probe_index].rgb;
    vec3 probe_pos = vec3(probe_grid_index) * grid_info.probe_grid_cell_size + grid_info.probe_grid_origin;
#ifdef USE_SAMPLE_OFFSET
    probe_pos.xz -= poiss_sample * grid_info.probe_grid_cell_size.xz * 0.25;
#endif

    vec3 probe_to_surfel = surface_position - probe_pos;

#ifdef USE_GRID_TAPER
    vec3 probe_irr;
    probe_life_weight *= sample_irradiance_probe(
      probe_irradiance_texture, irr_tex_info, grid_info, probe_index, normalize(probe_to_surfel), probe_irr);
#else
    vec3 probe_irr = sample_irradiance_probe(
      probe_irradiance_texture, irr_tex_info, grid_info, probe_index, normalize(probe_to_surfel));
#endif

    float weight = probe_life_weight;
    vec3 trilinear = mix(1.0 - cell_frac, cell_frac, vec3(probe_offset));

    {
      //  smooth backface test. attenuate irradiance for probes behind the surface (with respect to the surface normal).
      vec3 surfel_to_probe_n = normalize(probe_pos - surface_position);
      float backface_weight = max(0.0001, (dot(surfel_to_probe_n, surface_normal) + 1.0) * 0.5);
      backface_weight = backface_weight * backface_weight + 0.2;
      weight *= backface_weight;
    }

    if (params.visibility_test_enabled) {
      //  visibility test.
      vec3 vis_query_p = probe_to_surfel + (surface_normal + 3.0 * w_o) * params.normal_bias;
      vec3 vis_query_n = normalize(vis_query_p);

      vec2 depth_info = sample_depth_probe(probe_depth_texture, depth_tex_info, grid_info, probe_index, vis_query_n);
      float mean = depth_info.x;
      float var = abs(mean * mean - depth_info.y);

      float vis_probe_dist = length(vis_query_p);
      float normalizer = max(vis_probe_dist - mean, 0.0);
      float chebyshev_weight = var / (var + normalizer * normalizer);
      chebyshev_weight = clamp(pow(chebyshev_weight, 3.0), 0.0, 1.0);
      weight *= (vis_probe_dist <= mean) ? 1.0 : chebyshev_weight;
    }

    //  ensure non-zero weight.
    weight = max(0.0001, weight);

    const float crush_thresh = 0.2;
    if (weight < crush_thresh) {
      weight *= weight * weight * (1.0 / (crush_thresh * crush_thresh));
    }

    //  trilinear weight.
    weight *= trilinear.x * trilinear.y * trilinear.z;

#if LINEAR_BLENDING == 0
    probe_irr = sqrt(probe_irr);
#endif

    tot_weight += weight;
    tot_irradiance += probe_irr * weight;
  }

  tot_irradiance /= tot_weight;
#if LINEAR_BLENDING == 0
  tot_irradiance *= tot_irradiance;
#endif

#ifdef USE_GRID_TAPER
  tot_irradiance *= taper_weight;
#endif
  tot_irradiance *= ENERGY_CONSERVATION;
  return tot_irradiance;
}
#endif