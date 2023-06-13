#version 450 core

layout (location = 0) in vec4 v_proj_position;
layout (location = 1) in vec2 v_uv;

layout (location = 0) out vec4 frag_color;

layout (set = 0, binding = 0) uniform sampler2D scene_depth_texture;
layout (set = 0, binding = 1) uniform sampler3D opacity_texture;

#pragma include "cloud/billboard-data.glsl"

float weight_sample_depth() {
  //  check whether this position is behind any geometry,
  //  and attenuate the opacity sample if so. we use a depth reversing
  //  projection, so fully accept sample if the ray depth is *greater* than the scene depth.
  vec3 p_proj = v_proj_position.xyz / v_proj_position.w;
  vec2 p_sample = p_proj.xy * 0.5 + 0.5;
  float query_depth = p_proj.z;
  float sampled_depth = texture(scene_depth_texture, p_sample).r;
  const float bias = 0.0007;
  if (query_depth < sampled_depth + bias) {
    const float weight = 3000.0;
    return exp(-weight * ((sampled_depth + bias) - query_depth));
  } else {
    return 1.0;
  }
}

void main() {
  vec3 uv = vec3(v_uv, 0.0) + uvw_offset.xyz;

  float opacity_scale = translation_opacity_scale.w;
  float opacity = texture(opacity_texture, uv).r;
  float depth_scale = max(1.0 - scale_depth_test_enable.w, weight_sample_depth());
  opacity *= depth_scale * opacity_scale;
  opacity *= pow(max(0.0, (1.0 - length(v_uv * 2.0 - 1.0))), 2.0);

  frag_color = vec4(vec3(1.0), opacity);
}
