#version 450

#define SHADOW_UNIFORM_BUFFER_SET (0)
#define SHADOW_UNIFORM_BUFFER_BINDING (2)

#pragma include "shadow/data.glsl"
#pragma include "shadow/uniform-buffer.glsl"
#pragma include "shadow/sample.glsl"

layout (set = 0, binding = 3) uniform sampler2DArray sun_shadow_texture;

#pragma include "proc-tree/static-leaves-data.glsl"

layout (set = 1, binding = 0) uniform sampler2D material_texture;

layout (location = 0) in VS_OUT vs_in;
layout (location = 0) out vec4 frag_color;

#pragma include "toon-sun-light.glsl"
#pragma include "color/srgb-to-linear.glsl"

layout (push_constant, std140) uniform PushConstantData {
  vec4 aabb_p0_color_r;
  vec4 aabb_p1_color_g;
  vec4 model_scale_color_b;
  vec4 wind_fast_osc_scale_unused;  //  float, unused ...
};

float compute_shadow() {
  vec3 shadow_uvw;
  float shadow = simple_sample_shadow(
    vs_in.v_light_space_position0, vs_in.v_shadow_position, camera_position.xyz, view, sun_shadow_texture, shadow_uvw);
  return shadow;
}

vec3 extract_color() {
  return vec3(aabb_p0_color_r.w, aabb_p1_color_g.w, model_scale_color_b.w);
}

void main() {
  vec4 sampled = texture(material_texture, vs_in.v_uv);
#if 1
  if (sampled.a < 0.5 || length(vs_in.v_uv * 2.0 - 1.0) > 1.0) {
    discard;
  }
#else
  if (sampled.a < 0.5) {
    discard;
  }
#endif

  vec3 light = calculate_sun_light(normalize(vs_in.v_normal), normalize(sun_position.xyz), sun_color.rgb);
  float shadow = compute_shadow();
//  shadow = max(0.75, shadow);
  light = apply_sun_light_shadow(light, shadow);
#if 0
  sampled.rgb *= light;
#else

#if 1
  sampled.rgb = srgb_to_linear(extract_color());
#else
  sampled.rgb = srgb_to_linear(vec3(148.0/255.0, 231.0/255.0, 120.0/255.0)); //  green-ish
//  sampled.rgb = srgb_to_linear(vec3(255.0/255.0, 210.0/255.0, 132.0/255.0));  //  orange-ish
//  sampled.rgb = srgb_to_linear(vec3(255.0/255.0, 251.0/255.0, 132.0/255.0));  //  yellow
//  sampled.rgb = srgb_to_linear(vec3(255.0/255.0, 132.0/255.0, 132.0/255.0));  //  red
#endif
  float len = clamp(length(vs_in.v_uv * 2.0 - 1.0), 0.0, 1.0);
  sampled.rgb = mix(vec3(1.0, 1.0, 1.0), sampled.rgb, pow(len, 0.25)) * light;
#endif
  frag_color = vec4(sampled.rgb, 1.0);
}
