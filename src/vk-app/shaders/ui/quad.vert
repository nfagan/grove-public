#version 450

layout (location = 0) in vec4 xy_unused;
layout (location = 1) in vec4 instance_centroid_and_dimensions;
layout (location = 2) in vec4 instance_radius_fraction_and_border_size_and_opacity;
layout (location = 3) in uvec4 instance_color_and_border_color;

layout (push_constant, std140) uniform PushConstantData {
  vec4 framebuffer_dimensions;
};

layout (location = 0) out vec3 v_color;
layout (location = 1) out vec3 v_border_color;
layout (location = 2) out vec2 v_centroid_relative_position;
layout (location = 3) out vec2 v_dimensions;
layout (location = 4) out vec2 v_radius_and_border_size;
layout (location = 5) out float v_opacity;

vec4 pack_1u32_4fn(uint v) {
  float x = float(v & 0xffu);
  float y = float((v >> 8u) & 0xffu);
  float z = float((v >> 16u) & 0xffu);
  float w = float((v >> 24u) & 0xffu);
  return vec4(x, y, z, w) / 255.0;
}

float clamp_border_size() {
  float w = instance_centroid_and_dimensions.z;
  float h = instance_centroid_and_dimensions.w;
  float bs = instance_radius_fraction_and_border_size_and_opacity.y;
  return min(bs, min(w * 0.5, h * 0.5));
}

float clamp_radius() {
  float w = instance_centroid_and_dimensions.z;
  float h = instance_centroid_and_dimensions.w;
  float rf = clamp(instance_radius_fraction_and_border_size_and_opacity.x, 0.0, 1.0);
  return min(w, h) * 0.5 * rf;
}

void main() {
  vec2 xy = floor(xy_unused.xy + vec2(0.5)) / framebuffer_dimensions.xy * 2.0 - 1.0;
  //  vec2 xy = 2.0 * (xy_unused.xy + vec2(0.5)) / framebuffer_dimensions - 1.0;

  v_color = pack_1u32_4fn(instance_color_and_border_color.x).xyz;
  v_border_color = pack_1u32_4fn(instance_color_and_border_color.y).xyz;
  v_centroid_relative_position = xy_unused.xy - instance_centroid_and_dimensions.xy;
  v_dimensions = instance_centroid_and_dimensions.zw;
  v_radius_and_border_size = vec2(clamp_radius(), clamp_border_size());
  v_opacity = instance_radius_fraction_and_border_size_and_opacity.z;

  gl_Position = vec4(vec2(1.0, 1.0) * xy, 0.0, 1.0);
}
