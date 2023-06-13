vec3 compute_color(float x01, float z01, vec4 color_info, ivec3 color_component_indices) {
  float x11 = x01 * 2.0 - 1.0;
  float z11 = z01 * 2.0 - 1.0;

  float r = 1.0 - pow(z01, color_info.a);

  float midline_frac = 1.0 - pow(abs(x11), 2.0);
  float mid_atten = 1.0 - 0.25 * midline_frac;

  float center_dist = clamp(length(vec2(x11, z11)) / sqrt(2.0), 0.0, 1.0);
  float center_dist_amt = pow(center_dist, 0.25);

  vec3 color = vec3(r, mid_atten, center_dist_amt);
  vec3 color_scale = clamp(color_info.xyz, vec3(0.0), vec3(1.0));
  color = color * color_scale + (vec3(1.0) - color_scale);

  return vec3(color[color_component_indices.x],
              color[color_component_indices.y],
              color[color_component_indices.z]);
}