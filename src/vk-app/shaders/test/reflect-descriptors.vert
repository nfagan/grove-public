#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 position;
layout (location = 1) in vec2 uv;
layout (location = 2) in int index;

layout (location = 0) out vec2 v_uv;

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

void main() {
  v_uv = uv;
  gl_Position = vec4(position + data[index].field0.xyz + data[index].field1.xyz + again[index][0].field2.xyz, 1.0);
}