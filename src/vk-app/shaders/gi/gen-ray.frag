#version 450 core

#define NO_HIT (0xffffffffu)

#pragma include "gi/bvh-data.glsl"
#pragma include "gi/probe-data.glsl"

layout (location = 0) out vec4 out_ray_origin;
layout (location = 1) out vec4 out_ray_direction;
layout (location = 2) out vec4 out_ray_direction_bounce0;
layout (location = 3) out vec4 out_accum_ray_radiance;
layout (location = 4) out vec4 out_probe_ray_radiance;
layout (location = 5) out vec4 out_ray_depth_info;

layout (std430, set = 0, binding = 0) readonly buffer Primitives {
  Primitive primitives[];
};
layout (std430, set = 0, binding = 1) readonly buffer Instances {
  Instance instances[];
};

layout (std430, set = 0, binding = 2) readonly buffer ProbePositionIndices {
  ivec4 position_indices[];
};

layout (set = 0, binding = 3) uniform usampler2D ray_intersect_texture;
layout (set = 0, binding = 4) uniform sampler2D current_ray_direction_texture;
layout (set = 0, binding = 5) uniform sampler2D current_ray_direction_bounce0_texture;
layout (set = 0, binding = 6) uniform sampler2D current_accum_ray_radiance_texture;
layout (set = 0, binding = 7) uniform sampler2D random_direction_texture;
layout (set = 0, binding = 8) uniform sampler2D latest_hit_ray_radiance_texture;
layout (set = 0, binding = 9) uniform sampler2D current_ray_depth_info_texture;

layout (std140, push_constant) uniform PushConstantData {
  vec4 probe_counts;
  vec4 probe_grid_origin;
  vec4 probe_grid_cell_size;
  int ray_texture_dim;
  int rays_per_probe;
  int num_bounces;
  float max_probe_distance;
} uniform_data;

#pragma include "gi/index.glsl"
#pragma include "gi/oct.glsl"

ProbeGridInfo make_probe_grid_info() {
  ProbeGridInfo result;
  result.probe_counts = uniform_data.probe_counts.xyz;
  result.probe_grid_origin = uniform_data.probe_grid_origin.xyz;
  result.probe_grid_cell_size = uniform_data.probe_grid_cell_size.xyz;
  return result;
}

vec3 compute_normal(vec3 p0, vec3 p1, vec3 p2) {
  vec3 u = p1 - p0;
  vec3 v = p2 - p0;
  return normalize(cross(u, v));
}

vec3 barycentric_hit_coordinates(uvec2 encoded) {
  vec2 b = uintBitsToFloat(encoded);
  return vec3(b.x, b.y, max(0.0, 1.0 - b.x - b.y));
}

vec3 world_hit_position(vec3 p0, vec3 p1, vec3 p2, vec3 bary) {
  return p0 * bary.x + p1 * bary.y + p2 * bary.z;
}

vec3 probe_world_position(in ProbeGridInfo probe_grid_info, out bool modified) {
  int linear_ray_index = uniform_data.ray_texture_dim * int(gl_FragCoord.x) + int(gl_FragCoord.y);
  int linear_probe_index = linear_ray_index / uniform_data.rays_per_probe;
  ivec4 grid_info = position_indices[linear_probe_index];
  vec3 probe_pos = vec3(grid_info.xyz) * probe_grid_info.probe_grid_cell_size + probe_grid_info.probe_grid_origin;
  modified = grid_info.w > 0;
  return probe_pos;
}

void compute_hit_info(uint instance_index, uint primitive_index, vec3 bary_hit, out vec3 hit_p, out vec3 hit_n) {
  Instance instance = instances[instance_index];
  Primitive primitive = primitives[primitive_index];

  vec3 p0 = vec3(primitive.p0x, primitive.p0y, primitive.p0z);
  vec3 p1 = vec3(primitive.p1x, primitive.p1y, primitive.p1z);
  vec3 p2 = vec3(primitive.p2x, primitive.p2y, primitive.p2z);
  mat4 transform = instance_transform(instance);

  p0 = (transform * vec4(p0, 1.0)).xyz;
  p1 = (transform * vec4(p1, 1.0)).xyz;
  p2 = (transform * vec4(p2, 1.0)).xyz;

  hit_p = world_hit_position(p0, p1, p2, bary_hit);
  hit_n = compute_normal(p0, p1, p2);
}

void main() {
  ivec2 ray_uv = ivec2(gl_FragCoord.xy);

  uvec4 ray_hit_info = texelFetch(ray_intersect_texture, ray_uv, 0);
  //  Direction of the latest spawned ray.
  vec4 curr_ray_direction_info = texelFetch(current_ray_direction_texture, ray_uv, 0);
  //  Direction of the ray initially spawned from the probe.
  vec4 curr_ray_direction_bounce0_info = texelFetch(current_ray_direction_bounce0_texture, ray_uv, 0);
  //  Summed radiance over bounces.
  vec4 curr_ray_radiance = texelFetch(current_accum_ray_radiance_texture, ray_uv, 0);
  //  Radiance of last hit.
  vec4 latest_ray_radiance = texelFetch(latest_hit_ray_radiance_texture, ray_uv, 0);
  vec3 random_direction = texelFetch(random_direction_texture, ray_uv, 0).rgb;
  //  Depth information about the first ray spawned from the probe.
  vec4 curr_ray_depth_info = texelFetch(current_ray_depth_info_texture, ray_uv, 0);

  vec3 ray_direction = curr_ray_direction_info.xyz;
  vec3 ray_direction_bounce0 = curr_ray_direction_bounce0_info.xyz;

  float curr_bounce = curr_ray_direction_info.w;
  float curr_weight = curr_ray_direction_bounce0_info.w;
  float curr_tot_weight = curr_ray_radiance.w;

  uint primitive_index = ray_hit_info.x;
  uint instance_index = ray_hit_info.y;
  vec3 bary_hit = barycentric_hit_coordinates(ray_hit_info.zw);

  bool actually_hit = primitive_index != NO_HIT && dot(ray_direction, ray_direction) > 0.001;
  if (!actually_hit) {
    primitive_index = 0;
    instance_index = 0;
  }

  vec3 hit_n = vec3(0.0);
  vec3 hit_p = vec3(0.0);
  compute_hit_info(instance_index, primitive_index, bary_hit, hit_p, hit_n);

  bool probe_p_modified;
  vec3 probe_p = probe_world_position(make_probe_grid_info(), probe_p_modified);

  vec3 ro = probe_p;
  vec3 rd = random_direction;
  vec3 rd_bounce0 = ray_direction_bounce0;
  vec3 out_radiance = curr_ray_radiance.xyz + latest_ray_radiance.xyz * curr_weight;
  float out_bounce = curr_bounce + 1.0;
  vec4 out_p_radiance = vec4(0.0);
  float out_weight = 1.0;
  float out_tot_weight = max(1.0, curr_weight + curr_tot_weight);
  float converged = 0.0;

  if (probe_p_modified || out_bounce >= float(uniform_data.num_bounces)) {
    out_p_radiance.xyz = out_radiance / out_tot_weight;
    out_p_radiance.w = 1.0;
    out_radiance = vec3(0.0);
    out_bounce = 0.0;
    rd_bounce0 = rd;
    out_tot_weight = 0.0;
    converged = 1.0;

  } else {
    if (actually_hit && curr_weight > 0.0) {
      const float NORMAL_BIAS = 0.01;
      ro = hit_p + hit_n * NORMAL_BIAS;
      if (dot(rd, hit_n) < 0.0) {
        //  @TODO: Proper randomized direction in hemisphere around normal `hit_n`.
        rd = -rd;
      }
      out_weight = max(0.001, dot(rd, hit_n));  //  greater than 0 so that 0 can signify a terminated ray.
    } else {
      out_weight = 0.0;
    }
  }

  out_ray_origin = vec4(ro, 1.0);
  out_ray_direction = vec4(rd, out_bounce);
  out_ray_direction_bounce0 = vec4(rd_bounce0, out_weight);
  out_accum_ray_radiance = vec4(out_radiance, out_tot_weight);
  out_probe_ray_radiance = out_p_radiance;

  //  Update ray depth info.
  float probe_dist = curr_ray_depth_info.r;
  if (curr_bounce == 0.0) {
    probe_dist = uniform_data.max_probe_distance;

    if (actually_hit) {
      vec3 to_probe = probe_p - hit_p;
      probe_dist = length(to_probe);
    }
  }

  out_ray_depth_info = vec4(probe_dist, 1.0, 1.0, converged);
}