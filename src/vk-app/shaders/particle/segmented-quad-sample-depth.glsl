#version 450

#ifdef IS_VERTEX

layout (location = 0) in uvec4 position_color;
layout (location = 1) in vec4 min_depth_weight_unused;

layout (location = 0) out vec4 v_color;
layout (location = 1) out vec4 v_proj_position;
layout (location = 2) out float v_min_depth_weight;

#pragma include "pack/1u32_to_4fn.glsl"

layout (std140, push_constant) uniform PushConstantData {
  mat4 projection_view;
};

void main() {
  v_color = pack_1u32_4fn(position_color.w);
  v_proj_position = projection_view * vec4(uintBitsToFloat(position_color.xyz), 1.0);
  v_min_depth_weight = min_depth_weight_unused.x;
  gl_Position = v_proj_position;
}

#else

layout (location = 0) out vec4 frag_color;
layout (location = 0) in vec4 v_color;
layout (location = 1) in vec4 v_proj_position;
layout (location = 2) in float v_min_depth_weight;

layout (set = 0, binding = 0) uniform sampler2D scene_depth_image;

float linear_depth(float depth, float near, float far) {
  float z = depth * 2.0 - 1.0;
  return (2.0 * near * far) / (far + near - z * (far - near));
}

float weight_sample_depth() {
  //  check whether this position is behind any geometry, and attenuate the opacity sample if so.
  const float near = 0.1;
  const float far = 512.0;
  const float bias = 4.0;

  vec3 p_proj = v_proj_position.xyz / v_proj_position.w;

  vec2 p_sample = p_proj.xy * 0.5 + 0.5;
  float query_depth = linear_depth(1.0 - max(0.0, p_proj.z), near, far);

  float sampled_depth = texture(scene_depth_image, p_sample).r;
  float lin_depth = linear_depth(1.0 - max(0.0, sampled_depth), near, far);

  if (query_depth + bias > lin_depth) {
    const float weight = 1.0;
    return exp(-weight * ((query_depth + bias) - lin_depth));
  } else {
    return 1.0;
  }
}

void main() {
  float weighted_depth = max(v_min_depth_weight, weight_sample_depth());
  frag_color = vec4(v_color.xyz, weighted_depth * v_color.w);
}

#endif
