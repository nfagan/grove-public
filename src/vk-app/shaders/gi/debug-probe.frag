#version 450 core

#pragma include "gi/probe-data.glsl"

layout (location = 0) out vec4 frag_color;

layout (location = 0) in vec3 v_normal;
layout (location = 1) flat in float v_probe_index;

layout (set = 0, binding = 2) uniform sampler2D probe_irradiance_texture;

#pragma include "gi/debug-probe-data.glsl"

#pragma include "gi/index.glsl"
#pragma include "gi/oct.glsl"

#define IRRADIANCE_PROBE_ONLY

#pragma include "gi/sample-probes.glsl"

ProbeTextureInfo make_irr_probe_texture_info() {
  return decode_probe_texture_info(irr_texture_info0, irr_texture_info1);
}

ProbeGridInfo make_probe_grid_info() {
  ProbeGridInfo result;
  result.probe_counts = probe_counts.xyz;
  result.probe_grid_origin = probe_grid_origin.xyz;
  result.probe_grid_cell_size = probe_grid_cell_size.xyz;
  return result;
}

void main() {
  float irradiance_scale = sample_params0.x;
  bool check_finite = sample_params0.y == 1.0;

  ivec3 grid_index = linear_probe_index_to_grid_index(int(v_probe_index), ivec3(probe_counts.xyz));
  vec3 n = normalize(v_normal);
  vec3 indirect_irradiance = sample_irradiance_probe(
    probe_irradiance_texture, make_irr_probe_texture_info(), make_probe_grid_info(), grid_index, n);

  vec3 indirect_res = indirect_irradiance * irradiance_scale;
  frag_color = vec4(indirect_res, 1.0);

  if (check_finite) {
    bool any_inf = any(isinf(frag_color));
    bool any_nan = any(isnan(frag_color));
    float finite_val = any_inf || any_nan ? 1.0 : 0.0;
    frag_color = vec4(finite_val, finite_val, finite_val, 1.0);
  }
}
