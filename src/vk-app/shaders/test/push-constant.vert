#version 450

layout (location = 0) in vec3 position;

layout (push_constant) uniform PushConstants {
  vec4 data1;
  mat4 data2;
};

void main() {
  gl_Position = vec4(position, 1.0) + data1 * vec4(1.0);
}
