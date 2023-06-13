#ifndef NUM_SHADOW_SAMPLES
#error "Expected NUM_SHADOW_SAMPLES define"
#endif

//  https://gist.github.com/patriciogonzalezvivo/670c22f3966e662d2f83
vec4 mod289_(vec4 x) {
  return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 perm_(vec4 x) {
  return mod289_(((x * 34.0) + 1.0) * x);
}

float noise3_(vec3 p) {
  vec3 a = floor(p);
  vec3 d = p - a;
  d = d * d * (3.0 - 2.0 * d);

  vec4 b = a.xxyy + vec4(0.0, 1.0, 0.0, 1.0);
  vec4 k1 = perm_(b.xyxy);
  vec4 k2 = perm_(k1.xyxy + b.zzww);

  vec4 c = k2 + a.zzzz;
  vec4 k3 = perm_(c);
  vec4 k4 = perm_(c + 1.0);

  vec4 o1 = fract(k3 * (1.0 / 41.0));
  vec4 o2 = fract(k4 * (1.0 / 41.0));

  vec4 o3 = o2 * d.z + o1 * (1.0 - d.z);
  vec2 o4 = o3.yw * d.x + o3.xz * (1.0 - d.x);

  return o4.y * d.y + o4.x * (1.0 - d.y);
}

float csm_shadow_contribution(vec3 p_ws, sampler2DArray shadow_texture, vec3 uvw, float depth) {
#ifdef NO_PCF
  const float bias = 0.005;
  float sampled_depth = texture(shadow_texture, uvw).r;
  float shadow = float(depth > sampled_depth - bias ? 1.0 : 0.0);
  return clamp(shadow, 0.5, 1.0);
#else
  const float bias = 0.005;
  const float pcf_sample_scale = 1.0;

  float denom = float(NUM_SHADOW_SAMPLES);
  int noise_step = max(1, int(float(NUM_POISSON_SAMPLES) / denom));
  float pcf_step = pcf_sample_scale / float(textureSize(shadow_texture, 0).x);

  float shadow = 0.0;
  const float pi = 3.141592653589793;
  float theta = noise3_(p_ws) * pi;
  float ct = cos(theta);
  float st = sin(theta);
  mat2 r = mat2(vec2(ct, st), vec2(-st, ct));

  for (int i = 0; i < NUM_SHADOW_SAMPLES; i++) {
    int index = (noise_step * i) % NUM_POISSON_SAMPLES;
    vec2 uv_off = r * (POISSON_SAMPLES[index] * pcf_step);
    vec3 sample_uvw = vec3(uvw.xy + uv_off, uvw.z);
    float sampled_depth = texture(shadow_texture, sample_uvw).r;
    shadow += float(depth > sampled_depth - bias ? 1.0 : 0.0);
  }

  return clamp(shadow / denom, 0.5, 1.0);
#endif
}

float compute_camera_z_distance(in vec3 world_position, in vec3 cam_position, in mat4 cam_view) {
#if 0
  vec3 cam_front = normalize(vec3(cam_view[0][2], 0.0, cam_view[2][2]));
#else
  vec3 cam_front = normalize(vec3(cam_view[0][2], cam_view[1][2], cam_view[2][2]));
#endif
  return dot(cam_front, world_position - cam_position);
}

void compute_shadow_sample_info(in vec3 light_space_position0,
                                in float cam_z_dist,
                                out vec3 uvw,
                                out float in_layer_mask,
                                out float depth) {
  float u0 = cam_z_dist;
  float u1 = cam_z_dist - shadow_cascade_extents[0];
  float u2 = cam_z_dist - (shadow_cascade_extents[0] + shadow_cascade_extents[1]);

  vec3 pos = light_space_position0;
  float layer = 0.0;
  float layer_mask = 1.0;

  if (u0 >= 0.0 && u0 < shadow_cascade_extents[0]) {
    pos = (pos + shadow_cascade_uv_offsets[0].xyz) * shadow_cascade_uv_scales[0].xyz;
    layer = 0.0f;
    layer_mask = 0.0;

  } else if (u1 >= 0.0 && u1 < shadow_cascade_extents[1]) {
    pos = (pos + shadow_cascade_uv_offsets[1].xyz) * shadow_cascade_uv_scales[1].xyz;
    layer = 1.0f;
    layer_mask = 0.0;

  } else if (u2 >= 0.0 && u2 < shadow_cascade_extents[2]) {
    pos = (pos + shadow_cascade_uv_offsets[2].xyz) * shadow_cascade_uv_scales[2].xyz;
    layer = 2.0f;
    layer_mask = 0.0;
  }

  pos = pos * 0.5 + 0.5;
  uvw = vec3(pos.x, pos.y, layer);
  in_layer_mask = layer_mask;
  depth = pos.z;
}

float sample_shadow(vec3 p_ws, vec3 uvw, float in_layer_mask, float depth, sampler2DArray shadow_texture) {
  float shadow = csm_shadow_contribution(p_ws, shadow_texture, uvw, depth);
  return max(shadow, in_layer_mask);
}

float simple_sample_shadow(in vec3 position_ls,
                           in vec3 position_ws,
                           in vec3 cam_position,
                           in mat4 cam_view,
                           in sampler2DArray sun_shadow_texture,
                           out vec3 shadow_uvw) {
  float cam_z_dist = compute_camera_z_distance(position_ws, cam_position, cam_view);

  float in_shadow_layer_mask;
  float shadow_depth;
  compute_shadow_sample_info(position_ls, cam_z_dist, shadow_uvw, in_shadow_layer_mask, shadow_depth);

  return sample_shadow(position_ws, shadow_uvw, in_shadow_layer_mask, shadow_depth, sun_shadow_texture);
}

vec3 compute_shadow_cascade_visualization(in vec3 src_color, in float layer) {
  return mix(src_color, vec3(layer / float(NUM_SUN_SHADOW_CASCADES), 0.0, 0.0), 0.5);
}