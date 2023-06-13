//  include after probe-data.glsl
//  include before sample-probes.glsl

#ifndef SAMPLE_PROBE_SET
#error "Expected SAMPLE_PROBE_SET define"
#endif

#ifndef PROBE_POSITION_INDICES_BINDING
#error "Expected PROBE_POSITION_INDICES_BINDING define"
#endif

#ifndef PROBE_IRRADIANCE_BINDING
#error "Expected PROBE_IRRADIANCE_BINDING define"
#endif

#ifndef PROBE_DEPTH_BINDING
#error "Expected PROBE_DEPTH_BINDING define"
#endif

layout (std430, set = SAMPLE_PROBE_SET, binding = PROBE_POSITION_INDICES_BINDING) readonly buffer PositionIndices {
  ivec4 position_indices[];
};

layout (set = SAMPLE_PROBE_SET, binding = PROBE_IRRADIANCE_BINDING) uniform sampler2D probe_irradiance_texture;
layout (set = SAMPLE_PROBE_SET, binding = PROBE_DEPTH_BINDING) uniform sampler2D probe_depth_texture;

#ifndef PROBE_NO_UNIFORM_BUFFER

#ifndef PROBE_UNIFORM_BUFFER_BINDING
#error "Expected PROBE_UNIFORM_BUFFER_BINDING define"
#endif

layout (std140, set = SAMPLE_PROBE_SET, binding = PROBE_UNIFORM_BUFFER_BINDING) uniform ProbeSampleUniformData {
  ivec4 irr_probe_tex_info0;
  ivec4 irr_probe_tex_info1;  // probes_per_array_texture_dim, unused ...
  ivec4 depth_probe_tex_info0;
  ivec4 depth_probe_tex_info1;
  vec4 probe_counts;
  vec4 probe_grid_origin;
  vec4 probe_grid_cell_size;
  vec4 material_params0;  //  (bool) visibility_test_enabled, (float) normal_bias, (float) irradiance scale, unused
  vec4 camera_position;
} probe_uniform_data;

ProbeTextureInfo decode_irr_probe_texture_info() {
  return decode_probe_texture_info(probe_uniform_data.irr_probe_tex_info0, probe_uniform_data.irr_probe_tex_info1);
}

ProbeTextureInfo decode_depth_probe_texture_info() {
  return decode_probe_texture_info(probe_uniform_data.depth_probe_tex_info0, probe_uniform_data.depth_probe_tex_info1);
}

ProbeGridInfo decode_probe_grid_info() {
  ProbeGridInfo result;
  result.probe_counts = probe_uniform_data.probe_counts.xyz;
  result.probe_grid_origin = probe_uniform_data.probe_grid_origin.xyz;
  result.probe_grid_cell_size = probe_uniform_data.probe_grid_cell_size.xyz;
  return result;
}

ProbeSampleParams decode_probe_sample_params() {
  ProbeSampleParams result;
  result.visibility_test_enabled = probe_uniform_data.material_params0.x > 0.0;
  result.normal_bias = probe_uniform_data.material_params0.y;
  result.camera_position = probe_uniform_data.camera_position.xyz;
  return result;
}

float decode_irradiance_scale() {
  return probe_uniform_data.material_params0.z;
}

vec3 sample_probes(sampler2D probe_irradiance_texture, sampler2D probe_depth_texture,
                   ProbeTextureInfo irr_tex_info, ProbeTextureInfo depth_tex_info, ProbeGridInfo grid_info,
                   vec3 surface_position, vec3 surface_normal, ProbeSampleParams params);

vec3 sample_ddgi_probes(vec3 p, vec3 n) {
  return decode_irradiance_scale() * sample_probes(
    probe_irradiance_texture,
    probe_depth_texture,
    decode_irr_probe_texture_info(),
    decode_depth_probe_texture_info(),
    decode_probe_grid_info(),
    p,
    n,
    decode_probe_sample_params());
}

#endif  //  ifndef PROBE_NO_UNIFORM_BUFFER