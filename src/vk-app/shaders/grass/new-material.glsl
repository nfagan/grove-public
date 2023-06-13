#ifndef NEW_MAT_BINDING
#error "Expected NEW_MAT_BINDING definition"
#endif

struct NewMaterialParams {
  vec3 base_color0;
  vec3 base_color1;
  vec3 tip_color;
  float spec_scale;
  float spec_power;
  float overall_scale;
  float color_variation;
};

layout (std140, set = 0, binding = NEW_MAT_BINDING) uniform MaterialData {
  vec4 base_color0_spec_scale;
  vec4 base_color1_spec_power;
  vec4 tip_color_overall_scale;
  vec4 color_variation_unused;
} new_grass_data;

NewMaterialParams make_new_material_params(float spec_atten) {
  NewMaterialParams result;
  result.base_color0 = new_grass_data.base_color0_spec_scale.xyz;
  result.base_color1 = new_grass_data.base_color1_spec_power.xyz;
  result.tip_color = new_grass_data.tip_color_overall_scale.xyz;
  result.spec_scale = spec_atten * new_grass_data.base_color0_spec_scale.w;
  result.spec_power = new_grass_data.base_color1_spec_power.w;
  result.overall_scale = new_grass_data.tip_color_overall_scale.w;
  result.color_variation = new_grass_data.color_variation_unused.x;
  return result;
}

vec3 new_terrain_material_color(float splotch, float shadow, in NewMaterialParams params) {
  vec3 tmp_color = mix(params.base_color0, params.base_color1, mix(0.5, splotch, params.color_variation));
  tmp_color *= params.overall_scale;
  tmp_color *= shadow;
  return tmp_color;
}

vec3 new_material_color(float y, float splotch, vec3 v, vec3 sun_position, vec3 sun_color, float shadow, in NewMaterialParams params) {
  vec3 base_color = mix(params.base_color0, params.base_color1, mix(0.5, splotch, params.color_variation));

  const float max_specular = 1.0;
  vec3 l = normalize(sun_position);
  vec3 h = normalize(l + v);
  float hy = clamp(h.y, 0.0, max_specular);  //  1.0
  vec3 spec = params.tip_color * pow(hy, params.spec_power) * params.spec_scale;

  vec3 tmp_color = base_color;
#if 1
  float tip_atten = y * max(0.75, shadow);
  tip_atten = tip_atten * tip_atten * tip_atten;
//  tip_atten = pow(tip_atten, 2.0);
  tmp_color = tmp_color + sun_color * spec * tip_atten;
#else
  tmp_color = tmp_color + sun_color * spec * y;
#endif
  tmp_color *= params.overall_scale;
  tmp_color *= shadow;
  return tmp_color;
}
