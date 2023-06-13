#version 450
#extension GL_ARB_separate_shader_objects : enable

#define OUTPUT_ENABLED (0)

#if OUTPUT_ENABLED
layout (location = 0) out vec4 color;
#endif

void main() {
#if OUTPUT_ENABLED
  color = vec4(1.0, 0.0, 0.0, 0.0);
#endif
}
