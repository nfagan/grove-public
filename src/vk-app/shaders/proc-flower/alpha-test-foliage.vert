#version 450

/*

struct WindNode {
  Vec4<uint32_t> axis_root_info0;
  Vec4<uint32_t> axis_root_info1;
  Vec4<uint32_t> axis_root_info2;
  // .x = (u16) [p0.x, p0.y], .y = [p0.z, p1.x], .z = [p1.y, p1.z], .w = <unused>
  Vec4<uint32_t> aggregate_aabb0;
  // .x = (u16) [p0.x, p0.y], .y = [p0.z, p1.x], .z = [p1.y, p1.z], .w = <unused>
  Vec4<uint32_t> aggregate_aabb1;
  // .x = (u16) [p0.x, p0.y], .y = [p0.z, p1.x], .z = [p1.y, p1.z], .w = <unused>
  Vec4<uint32_t> node_positions;
};

struct Instance {
  WindNode wind_node;
  Vec4f translation_wind_node01;
  //  .x = [up_x, up_y, up_z, <unused>], 3 bytes
  //  .y = (float) scale
  //  .z = texture_layer
  Vec4<uint32_t> orientation_scale_texture_layer;
  Vec4<uint32_t> colors;
};

*/

struct ParsedInstance {
  vec3 translation;
  float wind_node01;
  vec3 orientation;
  float scale;
  uint texture_layer;
  float rand;
};

layout (location = 0) in vec2 position;

layout (location = 1) in uvec4 axis_root_info0;
layout (location = 2) in uvec4 axis_root_info1;
layout (location = 3) in uvec4 axis_root_info2;
layout (location = 4) in uvec4 aggregate_aabb0;
layout (location = 5) in uvec4 aggregate_aabb1;
layout (location = 6) in uvec4 node_positions;

layout (location = 7) in vec4 translation_wind_node01;
layout (location = 8) in uvec4 orientation_scale_texture_layer_randomness;
layout (location = 9) in uvec4 colors;

layout (location = 0) out vec2 v_uv;
layout (location = 1) out float v_texture_layer;
layout (location = 2) flat out uvec4 v_colors;

layout (set = 0, binding = 0) uniform sampler2D wind_displacement_texture;

layout (std140, push_constant) uniform PushConstantData {
  mat4 projection_view;
  vec4 world_aabb_p0_t;
  vec4 world_aabb_p1_unused;
  vec4 wind_world_bound_xz;
};

#pragma include "pi.glsl"
#pragma include "x_rotation.glsl"
#pragma include "rotation2.glsl"

#pragma include "pack/1u32_to_4fn.glsl"
#pragma include "pack/4u32_to_2vec3n.glsl"
#pragma include "frame.glsl"

#pragma include "wind.glsl"
#pragma include "proc-tree/wind.glsl"
#pragma include "proc-tree/roots-wind-displacement.glsl"

struct WindNode {
  uvec4 axis_root_info0;
  uvec4 axis_root_info1;
  uvec4 axis_root_info2;
  uvec4 aggregate_aabb0;
  uvec4 aggregate_aabb1;
  uvec4 node_positions;
};

WindNode make_wind_node() {
  WindNode result;
  result.axis_root_info0 = axis_root_info0;
  result.axis_root_info1 = axis_root_info1;
  result.axis_root_info2 = axis_root_info2;
  result.aggregate_aabb0 = aggregate_aabb0;
  result.aggregate_aabb1 = aggregate_aabb1;
  result.node_positions = node_positions;
  return result;
}

float wind_attenuation(float wind_strength) {
  const vec2 wind_displacement_limits = vec2(0.1, 0.3);
  const vec2 wind_strength_limits = vec2(0.03, 0.1);
  float a = smoothstep(wind_displacement_limits.x, wind_displacement_limits.y, wind_strength);
  return mix(wind_strength_limits.x, wind_strength_limits.y, a);
}

vec3 wind_translation(WindNode wind_node, float frac_node1, float t, vec3 world_min, vec3 world_max,
                      vec4 wind_world_bound_xz, sampler2D wind_texture) {
  //  Axis root info for node0 and node1
  vec4 node0_root_info0;
  vec4 node0_root_info1;
  vec4 node0_root_info2;
  vec4 node1_root_info0;
  vec4 node1_root_info1;
  vec4 node1_root_info2;
  unpack_axis_root_info(wind_node.axis_root_info0, node0_root_info0, node1_root_info0);
  unpack_axis_root_info(wind_node.axis_root_info1, node0_root_info1, node1_root_info1);
  unpack_axis_root_info(wind_node.axis_root_info2, node0_root_info2, node1_root_info2);

  //  Aggregate bounds for tree containing node0.
  vec3 aabb0_p0;
  vec3 aabb0_p1;
  pack_4u32_to_2vec3n(wind_node.aggregate_aabb0, aabb0_p0, aabb0_p1);
  //  Convert normalized coordinates to world coordinates.
  aabb0_p0 = mix(world_min, world_max, aabb0_p0);
  aabb0_p1 = mix(world_min, world_max, aabb0_p1);

  //  Aggregate bounds for tree containing node1.
  vec3 aabb1_p0;
  vec3 aabb1_p1;
  pack_4u32_to_2vec3n(wind_node.aggregate_aabb1, aabb1_p0, aabb1_p1);
  //  Convert normalized coordinates to world coordinates.
  aabb1_p0 = mix(world_min, world_max, aabb1_p0);
  aabb1_p1 = mix(world_min, world_max, aabb1_p1);

  vec2 ori_xz0 = mix(aabb0_p0, aabb0_p1, 0.5).xz;
  vec2 ori_xz1 = mix(aabb1_p0, aabb1_p1, 0.5).xz;

  //  Positions of nodes.
  vec3 node0_p;
  vec3 node1_p;
  pack_4u32_to_2vec3n(wind_node.node_positions, node0_p, node1_p);
  //  The node positions are normalized within their respective aabbs.
  node0_p = mix(aabb0_p0, aabb0_p1, node0_p);
  node1_p = mix(aabb1_p0, aabb1_p1, node1_p);

  vec2 wind0 = sample_wind_tip_displacement(ori_xz0, wind_world_bound_xz, wind_texture);
  vec2 wind1 = sample_wind_tip_displacement(ori_xz1, wind_world_bound_xz, wind_texture);

  float wind_atten0 = 0.5 * wind_attenuation(length(wind0));
  float wind_atten1 = 0.5 * wind_attenuation(length(wind1));

  vec3 p0 = wind_displacement(node0_p, t, aabb0_p0, aabb0_p1, node0_root_info0, node0_root_info1, node0_root_info2, wind_atten0);
  vec3 p1 = wind_displacement(node1_p, t, aabb1_p0, aabb1_p1, node1_root_info0, node1_root_info1, node1_root_info2, wind_atten1);
  return mix(p0, p1, frac_node1);
}

ParsedInstance parse_instance() {
  vec4 ori4 = pack_1u32_4fn(orientation_scale_texture_layer_randomness.x);

  ParsedInstance result;
  result.translation = translation_wind_node01.xyz;
  result.wind_node01 = translation_wind_node01.w;
  result.orientation = normalize(ori4.xyz * 2.0 - 1.0);
  result.scale = uintBitsToFloat(orientation_scale_texture_layer_randomness.y);
  result.texture_layer = orientation_scale_texture_layer_randomness.z & 0xffffu;
  result.rand = float((orientation_scale_texture_layer_randomness.z >> 16u) & 0xffffu) / float(0xffffu);
  return result;
}

void main() {
  ParsedInstance inst = parse_instance();
  WindNode wind_node = make_wind_node();

  vec3 world_aabb_p0 = world_aabb_p0_t.xyz;
  vec3 world_aabb_p1 = world_aabb_p1_unused.xyz;
  float t = world_aabb_p0_t.w;

  vec3 wind_trans = wind_translation(
    wind_node, inst.wind_node01, t, world_aabb_p0, world_aabb_p1, wind_world_bound_xz, wind_displacement_texture);

  mat3 ori = make_coordinate_system_y(inst.orientation);
#if 1
  vec3 pos = vec3(position.x, 0.0, position.y);
  pos.xz = rotation2(inst.rand * 2.0 * PI) * pos.xz;
  pos.xz = rotation2(sin(t * (4.0 - inst.rand * 2.0) + inst.rand * 2.0 * PI) * PI * 0.005) * pos.xz;
//  pos.y += (0.5 * sin(t * 3.0 + inst.rand * 2.0 * PI) * (position.y * 0.5 + 0.5) + 0.5) * 0.1;
#else
  vec3 pos = vec3(position, 0.0);
#endif
  vec3 p = ori * (pos * inst.scale) + inst.translation + wind_trans;

  v_uv = position * 0.5 + 0.5;
  v_texture_layer = float(inst.texture_layer);
  v_colors = colors;

  gl_Position = projection_view * vec4(p, 1.0);
}
