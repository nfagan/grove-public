#ifndef USE_ORIGINAL_SUN_LIGHT
//  @NOTE: 02/05/22
vec3 directional_light(vec3 light_position, vec3 position, vec3 camera_position, vec3 light_color,
                       float max_diffuse, float max_specular, float scale) {
  const float kd = 0.9;
  const float ks = 0.9;

  vec3 l = normalize(light_position - position);
  vec3 h = normalize(l + normalize(camera_position - position));
  float hy = clamp(h.y, 0.0, max_specular);  //  1.0
  vec3 spec = ks * light_color * pow(hy, 4.0) * scale;

  vec3 sun_dir = normalize(light_position);
  float dy = clamp(sun_dir.y, 0.0, max_diffuse); //  0.75
  vec3 diff = dy * kd * light_color * scale;

  return spec + diff;
}
#else
//  @NOTE: original, before 02/05/22
vec3 directional_light(vec3 light_position, vec3 position, vec3 camera_position, vec3 light_color, float diff_ao) {
  const float kd = 0.9;
  const float ks = 0.9;
  const vec3 up = vec3(0.0, 1.0, 0.0);

  vec3 direction = normalize(light_position - position);

  vec3 half_direction = normalize(direction + normalize(camera_position - position));
  float spec_strength = pow(max(dot(half_direction, up), 0.0), 4.0);
  vec3 spec = ks * light_color * spec_strength * diff_ao;

  vec3 sun_dir = normalize(light_position);
  vec3 diff = max(dot(sun_dir, up), 0.0) * kd * light_color * diff_ao;

  return spec + diff;
}
#endif