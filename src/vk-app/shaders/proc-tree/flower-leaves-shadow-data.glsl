layout (std140, set = GLOBAL_UNIFORM_SET, binding = GLOBAL_UNIFORM_BINDING) uniform GlobalUniformData {
  mat4 light_view_projection;
};

layout (std140, set = INSTANCE_UNIFORM_SET, binding = INSTANCE_UNIFORM_BINDING) uniform InstanceUniformData {
  PetalParameters petal_params;
  BlowingPetalParameters blowing_petal_params;

  vec4 aabb_p0_padded;
  vec4 aabb_p1_padded;
  vec4 model_scale_shadow_scale;

  vec4 color0;
  vec4 color1;
  vec4 color2;

  vec4 num_points;  //  x, z, unused ...
};