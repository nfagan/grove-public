#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) out vec4 frag_color;
layout (location = 0) in vec2 uv;

struct NestedNested {
  vec4 example[2];
};

struct Nested {
  NestedNested nested;
};

struct Data {
  vec4 field0;
  vec4 field1;
  Nested nested[2];
};

struct AnotherData {
  vec4 field2;
};

layout (std140, set = 0, binding = 1) uniform DataBlock {
  Data data[4];
  AnotherData again[3][2];
};

layout (set = 0, binding = 2) uniform sampler2D color_texture[2];

void main() {
  frag_color = texture(color_texture[0], uv);
}
