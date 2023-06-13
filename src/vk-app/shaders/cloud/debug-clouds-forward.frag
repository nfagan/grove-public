#version 450

#define INF_LIKE (10000000.0)

layout (location = 0) out vec4 frag_color;
layout (location = 0) in vec3 v_position;

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

vec3 evaluate_ray_within_aabb(vec3 ro, vec3 rd, float t) {
  vec3 p = clamp(ro + rd * t, volume_aabb_min.xyz, volume_aabb_max.xyz);
  return clamp((p - volume_aabb_min.xyz) / aabb_size(), vec3(0.0), vec3(1.0));
}

float opacity_sample(vec3 p0, vec3 p1, vec3 uvw_offset, vec3 uvw_scale, float density_scale) {
  const int num_cam_steps = 16;

  vec3 cam_step = (p1 - p0) / float(num_cam_steps);
  float cam_step_size = length(cam_step);

  vec3 uvw = p0 + uvw_offset;
  vec3 uvw_step = cam_step * uvw_scale;
  float linear_distance = 0.0;

  for (int i = 0; i < num_cam_steps; i++) {
    float samp = texture(volume_texture, uvw).r;
    linear_distance += samp * cam_step_size;
    uvw += uvw_step;
  }

  float transmittance = exp(-linear_distance * density_scale);
  return 1.0 - transmittance;
}

void main() {
  vec3 camera_position = camera_position4.xyz;
  vec3 uvw_offset = uvw_offset_density_scale.xyz;
  vec3 uvw_scale = uvw_scale_depth_test_enable.xyz;
  float density_scale = uvw_offset_density_scale.w;
  vec4 world_pos_w = (inv_view_proj * vec4(v_position.x, v_position.y, 1.0, 1.0));
  vec3 world_pos = world_pos_w.xyz / world_pos_w.w;
  vec3 from_camera = world_pos - camera_position;

  float fade = 1.0;

  vec3 ray_ws = normalize(from_camera);
  float aabb_t0;
  float aabb_t1;
  bool intersects = ray_aabb_intersect(ray_ws, camera_position, aabb_t0, aabb_t1);

  frag_color = vec4(0.0);

  if (intersects && aabb_t1 >= 0.0) {
    aabb_t0 = max(0.0, aabb_t0);

    vec3 p0 = evaluate_ray_within_aabb(camera_position, ray_ws, aabb_t0);
    vec3 p1 = evaluate_ray_within_aabb(camera_position, ray_ws, aabb_t1);
    float alpha = opacity_sample(p0, p1, uvw_offset, uvw_scale, density_scale);
    frag_color = vec4(cloud_color.xyz, alpha * fade);
  }
}