#version 450

layout (location = 0) in vec2 v_uv;
layout (location = 1) flat in uint v_texture_layer;

layout (location = 0) out vec4 frag_color;

layout (set = 0, binding = 0) uniform sampler2DArray material_texture;

void main() {
  vec3 uvw = vec3(v_uv, float(v_texture_layer));
  vec3 color = texture(material_texture, uvw).rgb;
  frag_color = vec4(color, 1.0);
}
