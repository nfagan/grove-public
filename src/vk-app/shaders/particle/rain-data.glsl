layout (std140, set = GLOBAL_UNIFORM_SET, binding = GLOBAL_UNIFORM_BINDING) uniform GlobalUniformData {
  mat4 projection;
  mat4 view;
  vec4 particle_scale_alpha_scale;
};
