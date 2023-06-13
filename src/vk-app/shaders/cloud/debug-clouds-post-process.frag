#version 450

#define INF_LIKE (10000000.0)

layout (location = 0) out vec4 frag_color;
layout (location = 0) in vec3 v_position;

layout (push_constant) uniform PushConstantData {
  mat4 projection;
  mat4 view;
};

layout (std140, set = 0, binding = 0) uniform GlobalUniformData {
  mat4 inv_view_proj;
  vec4 camera_position4;
  vec4 cloud_color;
};

layout (std140, set = 1, binding = 0) uniform InstanceUniformData {
  vec4 uvw_offset_density_scale;
  vec4 uvw_scale_depth_test_enable;
  vec4 volume_aabb_min;
  vec4 volume_aabb_max;
};

layout (set = 0, binding = 1) uniform sampler2D scene_depth_texture;
#ifndef NO_COLOR_IMAGE
layout (set = 0, binding = 2) uniform sampler2D scene_color_texture;
#endif
layout (set = 1, binding = 1) uniform sampler3D volume_texture;

vec3 aabb_size() {
  return (volume_aabb_max - volume_aabb_min).xyz;
}

vec2 ray_aabb_axis(float rd, float ro, float aabb_min, float aabb_max) {
  float inv_d = 1.0 / rd;
  float t00 = (aabb_min - ro) * inv_d;
  float t11 = (aabb_max - ro) * inv_d;

  if (t00 > t11) {
    return vec2(t11, t00);
  } else {
    return vec2(t00, t11);
  }
}

bool ray_aabb_intersect(vec3 rd, vec3 ro, out float t0, out float t1) {
  t0 = -1.0;
  t1 = 1.0;

  float tmp_t0 = -INF_LIKE;
  float tmp_t1 = INF_LIKE;

  vec2 x_check = ray_aabb_axis(rd.x, ro.x, volume_aabb_min.x, volume_aabb_max.x);
  vec2 y_check = ray_aabb_axis(rd.y, ro.y, volume_aabb_min.y, volume_aabb_max.y);
  vec2 z_check = ray_aabb_axis(rd.z, ro.z, volume_aabb_min.z, volume_aabb_max.z);

  tmp_t0 = max(tmp_t0, x_check.x);
  tmp_t0 = max(tmp_t0, y_check.x);
  tmp_t0 = max(tmp_t0, z_check.x);

  tmp_t1 = min(tmp_t1, x_check.y);
  tmp_t1 = min(tmp_t1, y_check.y);
  tmp_t1 = min(tmp_t1, z_check.y);

  if (tmp_t0 > tmp_t1) {
    return false;

  } else {
    t0 = tmp_t0;
    t1 = tmp_t1;

    return true;
  }
}

vec3 clamp_aabb(vec3 p) {
  return clamp((p - volume_aabb_min.xyz) / aabb_size(), vec3(0.0), vec3(1.0));
}

float weight_sample_depth(vec3 p_view) {
  //  check whether this position is behind any geometry,
  //  and attenuate the opacity sample if so. we use a depth reversing
  //  projection, so fully accept sample if the ray depth is *greater* than the scene depth.
  vec4 p_proj_w = projection * vec4(p_view, 1.0);
  vec3 p_proj = p_proj_w.xyz / p_proj_w.w;
  vec2 p_sample = p_proj.xy * 0.5 + 0.5;
  float query_depth = p_proj.z;
  float sampled_depth = texture(scene_depth_texture, p_sample).r;
  if (query_depth < sampled_depth) {
    const float weight = 2000.0;
    return exp(-weight * (sampled_depth - query_depth));
  } else {
    return 1.0;
  }
}

void main() {
  vec3 camera_position = camera_position4.xyz;
  vec3 uvw_offset = uvw_offset_density_scale.xyz;
  float density_scale = uvw_offset_density_scale.w;

  vec3 uvw_scale = uvw_scale_depth_test_enable.xyz;
  float depth_test_enable = uvw_scale_depth_test_enable.w;

  vec4 world_pos_w = (inv_view_proj * vec4(v_position.x, v_position.y, 1.0, 1.0));
  vec3 world_pos = world_pos_w.xyz / world_pos_w.w;

  vec3 from_camera = world_pos - camera_position;
  vec3 rd_ws = normalize(from_camera);
  float aabb_t0;
  float aabb_t1;
  bool intersects = ray_aabb_intersect(rd_ws, camera_position, aabb_t0, aabb_t1);

#ifdef NO_COLOR_IMAGE
  frag_color = vec4(0.0);
#else
  frag_color = texelFetch(scene_color_texture, ivec2(gl_FragCoord.xy), 0);
#endif

  if (intersects && aabb_t1 >= 0.0) {
    aabb_t0 = max(0.0, aabb_t0);

    vec3 world_p0 = clamp(camera_position + rd_ws * aabb_t0, volume_aabb_min.xyz, volume_aabb_max.xyz);
    vec3 world_p1 = clamp(camera_position + rd_ws * aabb_t1, volume_aabb_min.xyz, volume_aabb_max.xyz);
    vec3 tex_p0 = clamp_aabb(world_p0);
    vec3 tex_p1 = clamp_aabb(world_p1);

    vec3 view_p0 = (view * vec4(world_p0, 1.0)).xyz;
    vec3 view_p1 = (view * vec4(world_p1, 1.0)).xyz;
    vec3 view_rd = (view * vec4(rd_ws, 0.0)).xyz;

    const int num_cam_steps = 16;
    vec3 cam_step = (tex_p1 - tex_p0) / float(num_cam_steps);
    float cam_step_size = length(cam_step);
    vec3 tex_step = cam_step * uvw_scale;
    vec3 view_step = (view_p1 - view_p0) / float(num_cam_steps);
    float view_step_size = length(view_step);
    float depth_test_mask = 1.0 - depth_test_enable;

    vec3 p_view = view_p0;
    vec3 p_tex = tex_p0 + uvw_offset;

    float linear_distance = 0.0;
    for (int i = 0; i < num_cam_steps; i++) {
      float rand = sin(length(p_view) * 2048.0);
      vec3 rand_off = view_rd * 0.01 * view_step_size * rand;

      float samp = texture(volume_texture, p_tex).r;
      float ib_mask = max(depth_test_mask, weight_sample_depth(p_view + rand_off));
      linear_distance += samp * cam_step_size * ib_mask;
      p_tex += tex_step;
      p_view += view_step;
    }

    float rand_scale = 1.0 + sin(length(v_position * 32.0) * 2048.0) * 0.025;
    float transmittance = exp(-linear_distance * density_scale * rand_scale);
    float alpha = 1.0 - transmittance;
#ifdef NO_COLOR_IMAGE
    frag_color = vec4(cloud_color.rgb, alpha);
#else
    frag_color.rgb = mix(frag_color.rgb, cloud_color.rgb, alpha);
#endif
  }
}