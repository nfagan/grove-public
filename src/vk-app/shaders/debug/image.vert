#version 450

layout (location = 0) in vec2 position;
layout (location = 0) out vec2 v_uv;
layout (location = 1) out float v_min_alpha;

layout (push_constant) uniform PushConstantData {
  vec4 translation_scale;
  vec4 viewport_dims_image_dims;
  vec4 min_alpha;
};

void main() {
  vec2 translation = translation_scale.xy;
  vec2 scale = translation_scale.zw;
  vec2 viewport_dims = viewport_dims_image_dims.xy;
  vec2 image_dims = viewport_dims_image_dims.zw;

  vec2 p = position;
  float viewport_ar = viewport_dims.x / viewport_dims.y;
  float image_ar = image_dims.x / image_dims.y;

  p.x /= viewport_ar;
  p.x *= image_ar;
  p *= scale;
  p += translation;

  v_uv = position * 0.5 + 0.5;
  v_min_alpha = min_alpha.x;
  gl_Position = vec4(p, 1.0, 1.0);
}
