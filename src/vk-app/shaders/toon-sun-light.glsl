vec3 calculate_sun_light(vec3 n, vec3 l, vec3 sun_color) {
#if 0
  float d = max(dot(n, l), 0.0);
  vec3 res = clamp(d * sun_color * 4.0, 0.5, 1.0);
  return res;
#else
//  return clamp(max(dot(n, l), 0.0) * 4.0, 0.5, 1.0) * sun_color;
  return clamp(max(dot(n, l), 0.0) * 4.0, 0.0, 1.0) * sun_color * 0.5 + 0.5;
#endif
}

vec3 apply_sun_light_shadow(vec3 light, float shadow) {
#if 1
  return clamp(light * shadow, vec3(0.5), vec3(1.0));
#else
  return clamp(light * shadow, 0.0, 1.0) * 0.5 + 0.5;
#endif
}