#version 450

vec2 positions[3] = vec2[](
  vec2(-1.0, -1.0),
  vec2(-1.0, 3.0),
  vec2(3.0, -1.0)
);

#ifdef SAMPLE_LINEAR
layout (location = 0) out vec2 v_uv;
#endif

void main() {
#ifdef SAMPLE_LINEAR
  v_uv = positions[gl_VertexIndex] * 0.5 + 0.5;
#endif
  gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}
