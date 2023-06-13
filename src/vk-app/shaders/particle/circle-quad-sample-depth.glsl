#version 450

#ifdef IS_VERTEX

layout (location = 0) in vec2 position;
layout (location = 1) in vec4 translation_scale;
layout (location = 2) in vec4 color;

layout (location = 0) out vec4 v_proj_position;
layout (location = 1) out vec4 v_color;
layout (location = 2) out vec2 v_position11;

layout (std140, push_constant) uniform PushConstantData {
  mat4 projection_view;
  mat4 inv_view;
};

void main() {
  vec3 trans = translation_scale.xyz;
  float scale = translation_scale.w;
  vec3 scaled_pos = mat3(inv_view) * scale * vec3(position, 0.0);

  v_color = color;
  v_proj_position = projection_view * vec4(scaled_pos + trans, 1.0);
  v_position11 = position;

  gl_Position = v_proj_position;
}

#else

layout (location = 0) out vec4 frag_color;

layout (location = 0) in vec4 v_proj_position;
layout (location = 1) in vec4 v_color;
layout (location = 2) in vec2 v_position11;

layout (set = 0, binding = 0) uniform sampler2D scene_depth_image;

float linear_depth(float depth, float near, float far) {
  float z = depth * 2.0 - 1.0;
  return (2.0 * near * far) / (far + near - z * (far - near));
}

float weight_sample_depth() {
  //  check whether this position is behind any geometry, and attenuate the opacity sample if so.
  const float near = 0.1;
  const float far = 512.0;
  const float bias = 0.5;

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
  float r = length(v_position11);
  if (length(r) >= 1.0) {
    discard;
  }

//  const float a = 0.0625;
  const float a = 0.25;
  float atten = clamp(max(0.0, r - (1.0 - a)) / a, 0.0, 1.0);
  float alpha_atten = 1.0 - atten * atten;

  float weighted_depth = weight_sample_depth();
  frag_color = vec4(v_color.xyz, weighted_depth * v_color.w * alpha_atten);
}

#endif
