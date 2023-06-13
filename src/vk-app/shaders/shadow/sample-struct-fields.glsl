#ifndef NUM_SUN_SHADOW_CASCADES
#error "Expected NUM_SUN_SHADOW_CASCADES define"
#endif

vec4 shadow_cascade_extents;
vec4 shadow_cascade_uv_scales[NUM_SUN_SHADOW_CASCADES];
vec4 shadow_cascade_uv_offsets[NUM_SUN_SHADOW_CASCADES];