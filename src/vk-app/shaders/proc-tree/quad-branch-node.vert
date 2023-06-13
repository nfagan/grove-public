#version 450

layout (location = 0) in vec2 position;
layout (location = 1) in uint instance_index;

struct RenderBranchNodeDynamicData {
  vec4 self_p_self_r;
  vec4 child_p_child_r;
};

struct RenderBranchNodeStaticData {
  uvec4 directions0;
  uvec4 directions1;
  uvec4 aggregate_index_unused;
};

struct RenderWindBranchNodeStaticData {
  uvec4 directions0;
  uvec4 directions1;
  uvec4 aggregate_index_unused;
  uvec4 wind_info0;
  uvec4 wind_info1;
  uvec4 wind_info2;
};

struct VS_OUT {
  vec3 normal;
  vec3 light_space_position0;
  vec3 shadow_position;
};

layout (set = 0, binding = 0, std140) uniform UniformData {
  vec4 num_points_xz_t;
  vec4 wind_displacement_info;  //  vec2 wind_displacement_limits, vec2 wind_strength_limits
  vec4 wind_world_bound_xz;
//  Shadow info.
  mat4 view;
  mat4 sun_light_view_projection0;
  vec4 shadow_info; //  min_radius_shadow, max_radius_scale_shadow, unused, unused
//  Frag info.
  vec4 sun_position;
  vec4 sun_color;
  vec4 camera_position;
  vec4 color;
  #pragma include "shadow/sample-struct-fields.glsl"
};

layout (set = 0, binding = 1, std430) readonly buffer DynamicInstances {
  RenderBranchNodeDynamicData dynamic_instances[];
};

layout (push_constant, std140) uniform PushConstantData {
  mat4 projection_view;
};

#ifndef IS_SHADOW
layout (location = 0) out VS_OUT vs_out;
#endif

vec3 instance_scale(float radius) {
  return vec3(radius, 1.0, radius);
}

void main() {
  RenderBranchNodeDynamicData dyn_inst = dynamic_instances[instance_index];

  float self_radius = dyn_inst.self_p_self_r.w;
  vec3 self_p = dyn_inst.self_p_self_r.xyz;
  float child_radius = dyn_inst.child_p_child_r.w;
  vec3 child_p = dyn_inst.child_p_child_r.xyz;

  float x11 = position.x;
  float y01 = position.y * 0.5 + 0.5;
  float y = y01;

  mat3 inv_view = transpose(mat3(view));

  vec3 self_x = inv_view[0];
  vec3 child_x = inv_view[0];

  vec3 p0 = self_x * x11 * instance_scale(self_radius) + self_p;
  vec3 p1 = child_x * x11 * instance_scale(child_radius) + child_p;
  vec3 p = mix(p0, p1, y);

#ifndef IS_SHADOW
  vec3 n_self = normalize(p0 - self_p);
  vec3 n_child = normalize(p1 - child_p);
  vec3 n = mix(n_self, n_child, y);

  vs_out.normal = n;
  vs_out.light_space_position0 = vec3(sun_light_view_projection0 * vec4(p, 1.0));
  vs_out.shadow_position = p;
#endif

  gl_Position = projection_view * vec4(p, 1.0);
#ifdef IS_SHADOW
  gl_Position.z = gl_Position.z * 0.5 + 0.5;
#endif
}
