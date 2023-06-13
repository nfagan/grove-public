#ifndef NUM_SUN_SHADOW_CASCADES
#error "Expected NUM_SUN_SHADOW_CASCADES define"
#endif

layout (set = SHADOW_UNIFORM_BUFFER_SET, binding = SHADOW_UNIFORM_BUFFER_BINDING, std140) uniform ShadowData {
  vec4 shadow_cascade_extents;
  vec4 shadow_cascade_uv_scales[NUM_SUN_SHADOW_CASCADES];
  vec4 shadow_cascade_uv_offsets[NUM_SUN_SHADOW_CASCADES];
};