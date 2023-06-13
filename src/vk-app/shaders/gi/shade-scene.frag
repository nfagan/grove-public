#version 450 core

#define NO_HIT (0xffffffffu)
#define NON_CLAMPED_SHADOW
#define ALPHA_TEST (0)
#define ALPHA_CLIP_THRESHOLD (0.5)

#pragma include "gi/bvh-data.glsl"
#pragma include "gi/probe-data.glsl"

layout (location = 0) out vec4 radiance;

struct AuxPrimitive {
  float u;
  float v;
#ifdef INCLUDE_NORMAL_IN_AUX_PRIMITIVE
  float nx;
  float ny;
  float nz;
#endif
  uint material_index;
};

layout (std430, set = 0, binding = 0) readonly buffer Primitives {
  Primitive primitives[];
};

layout (std430, set = 0, binding = 1) readonly buffer Instances {
  Instance instances[];
};

layout (std430, set = 0, binding = 2) readonly buffer AuxPrimitives {
  AuxPrimitive aux_primitives[];
};

layout (set = 0, binding = 3) uniform usampler2D ray_intersect_texture;
layout (set = 0, binding = 4) uniform sampler2DArray color_texture;

#define SAMPLE_PROBE_SET (0)
#define PROBE_POSITION_INDICES_BINDING (5)
#define PROBE_IRRADIANCE_BINDING (6)
#define PROBE_DEPTH_BINDING (7)
#define PROBE_NO_UNIFORM_BUFFER

#pragma include "gi/sample-probes-data.glsl"

struct MaterialParams {
  int material_type;
  int toon_shade;
  int use_shadow;
  int check_finite;
  float irradiance_scale;
};

layout (std140, set = 0, binding = 8) uniform UniformData {
  vec4 probe_counts;
  vec4 probe_grid_origin;
  vec4 probe_grid_cell_size;

  vec4 camera_position;
  vec4 sun_position;
  vec4 sun_color;
  vec4 constant_ambient;
  vec4 visibility_test_enabled_normal_bias; //  bool, float, unused, unsed

  ivec4 irr_probe_tex_info0;
  ivec4 irr_probe_tex_info1;  // probes_per_array_texture_dim, unused ...
  ivec4 depth_probe_tex_info0;
  ivec4 depth_probe_tex_info1;

  ivec4 material_params0; //  material_type, toon_shade, use_shadow, check_finite
  vec4 material_params1;  //  irradiance scale, unused ...

  mat4 sun_light_view_projection0;
  mat4 view;
} uniform_data;

#define SHADOW_UNIFORM_BUFFER_SET (0)
#define SHADOW_UNIFORM_BUFFER_BINDING (9)

layout (set = 0, binding = 10) uniform sampler2DArray sun_shadow_texture;

#pragma include "shadow/uniform-buffer.glsl"
#pragma include "shadow/data.glsl"
#pragma include "shadow/sample.glsl"

#pragma include "gi/index.glsl"
#pragma include "gi/oct.glsl"
#pragma include "gi/sample-probes.glsl"

#pragma include "toon-sun-light.glsl"

MaterialParams make_material_params() {
  MaterialParams result;
  result.material_type = uniform_data.material_params0[0];
  result.toon_shade = uniform_data.material_params0[1];
  result.use_shadow = uniform_data.material_params0[2];
  result.check_finite = uniform_data.material_params0[3];
  result.irradiance_scale = uniform_data.material_params1[0];
  return result;
}

ProbeTextureInfo make_irr_probe_texture_info() {
  return decode_probe_texture_info(uniform_data.irr_probe_tex_info0, uniform_data.irr_probe_tex_info1);
}

ProbeTextureInfo make_depth_probe_texture_info() {
  return decode_probe_texture_info(uniform_data.depth_probe_tex_info0, uniform_data.depth_probe_tex_info1);
}

ProbeGridInfo make_probe_grid_info() {
  ProbeGridInfo result;
  result.probe_counts = uniform_data.probe_counts.xyz;
  result.probe_grid_origin = uniform_data.probe_grid_origin.xyz;
  result.probe_grid_cell_size = uniform_data.probe_grid_cell_size.xyz;
  return result;
}

ProbeSampleParams make_probe_sample_params() {
  ProbeSampleParams result;
  result.visibility_test_enabled = uniform_data.visibility_test_enabled_normal_bias.x > 0.0;
  result.normal_bias = uniform_data.visibility_test_enabled_normal_bias.y;
  result.camera_position = uniform_data.camera_position.xyz;
  return result;
}

vec3 compute_normal(vec3 p0, vec3 p1, vec3 p2) {
  vec3 u = p1 - p0;
  vec3 v = p2 - p0;
  return normalize(cross(u, v));
}

vec2 compute_uv_from_aux_primitives(AuxPrimitive ap0, AuxPrimitive ap1, AuxPrimitive ap2, vec3 bary_hit) {
  vec2 uv0 = vec2(ap0.u, ap0.v);
  vec2 uv1 = vec2(ap1.u, ap1.v);
  vec2 uv2 = vec2(ap2.u, ap2.v);
  return uv0 * bary_hit.x + uv1 * bary_hit.y + uv2 * bary_hit.z;
}

vec3 barycentric_hit_coordinates(uvec2 encoded) {
  vec2 b = uintBitsToFloat(encoded);
  return vec3(b.x, b.y, max(0.0, 1.0 - b.x - b.y));
}

vec3 world_hit_position(vec3 p0, vec3 p1, vec3 p2, vec3 bary) {
  return p0 * bary.x + p1 * bary.y + p2 * bary.z;
}

float compute_shadow(vec3 world_p) {
  vec3 proj_p = (uniform_data.sun_light_view_projection0 * vec4(world_p, 1.0)).xyz;
  vec3 shadow_uvw;
  return simple_sample_shadow(proj_p, world_p, uniform_data.camera_position.xyz, uniform_data.view, sun_shadow_texture, shadow_uvw);
}

vec3 direct_sun_light(vec3 n, float sun_shadow, MaterialParams material_params) {
  vec3 l = normalize(uniform_data.sun_position.xyz);
  if (material_params.toon_shade == 1) {
    return apply_sun_light_shadow(calculate_sun_light(n, l, uniform_data.sun_color.xyz), sun_shadow);
  } else {
    return max(0.0, dot(n, l)) * uniform_data.sun_color.xyz * sun_shadow;
  }
}

vec3 sample_probes(vec3 p, vec3 n) {
  return sample_probes(
    probe_irradiance_texture,
    probe_depth_texture,
    make_irr_probe_texture_info(),
    make_depth_probe_texture_info(),
    make_probe_grid_info(),
    p, n, make_probe_sample_params());
}

void main() {
  uvec4 ray_hit_info = texelFetch(ray_intersect_texture, ivec2(gl_FragCoord.xy), 0);

  uint primitive_index = ray_hit_info.x;
  uint instance_index = ray_hit_info.y;
  vec3 bary_hit = barycentric_hit_coordinates(ray_hit_info.zw);

  float hit_mask = 1.0;
  float mat_mask = 1.0;
  uint material_index = 0;
  bool actually_hit = primitive_index != NO_HIT;

  if (!actually_hit) {
    primitive_index = 0;
    instance_index = 0;
    material_index = 0;
    hit_mask = 0.0;
  }

  MaterialParams material_params = make_material_params();

  Instance instance = instances[instance_index];
  Primitive primitive = primitives[primitive_index];

  AuxPrimitive aux_primitive0 = aux_primitives[primitive.aux_index + 0];
  AuxPrimitive aux_primitive1 = aux_primitives[primitive.aux_index + 1];
  AuxPrimitive aux_primitive2 = aux_primitives[primitive.aux_index + 2];

  float texture_layer = 0.0;
  if (material_params.material_type == 0) {
    texture_layer = float(aux_primitive0.material_index);
  } else {
    mat_mask = 0.0;
  }

  vec2 uv = compute_uv_from_aux_primitives(aux_primitive0, aux_primitive1, aux_primitive2, bary_hit);
  vec3 uvw = vec3(uv, texture_layer);
#if ALPHA_TEST
  vec4 color_sample = texture(color_texture, uvw);
  vec3 color = color_sample.rgb;
  mat_mask *= float(color_sample.a >= ALPHA_CLIP_THRESHOLD);
#else
  vec3 color = texture(color_texture, uvw).rgb;
#endif

  vec3 p0 = vec3(primitive.p0x, primitive.p0y, primitive.p0z);
  vec3 p1 = vec3(primitive.p1x, primitive.p1y, primitive.p1z);
  vec3 p2 = vec3(primitive.p2x, primitive.p2y, primitive.p2z);
  mat4 transform = instance_transform(instance);

  p0 = (transform * vec4(p0, 1.0)).xyz;
  p1 = (transform * vec4(p1, 1.0)).xyz;
  p2 = (transform * vec4(p2, 1.0)).xyz;

#ifdef INCLUDE_NORMAL_IN_AUX_PRIMITIVE
  vec3 n0 = vec3(aux_primitive0.nx, aux_primitive0.ny, aux_primitive0.nz);
  vec3 n1 = vec3(aux_primitive1.nx, aux_primitive1.ny, aux_primitive1.nz);
  vec3 n2 = vec3(aux_primitive2.nx, aux_primitive2.ny, aux_primitive2.nz);
  vec3 n = normalize(n0 * bary_hit.x + n1 * bary_hit.y + n2 * bary_hit.z);
#else
  vec3 n = compute_normal(p0, p1, p2);
#endif

  vec3 hit_position = world_hit_position(p0, p1, p2, bary_hit);
  vec3 indirect_irradiance = sample_probes(hit_position, n);
  float sun_shadow = compute_shadow(hit_position);
  if (material_params.use_shadow == 0) {
    sun_shadow = 1.0;
  }

  vec3 diff = direct_sun_light(n, sun_shadow, material_params) * color;
  //  Sum of direct and indirect.
  radiance = vec4(diff + material_params.irradiance_scale * indirect_irradiance, 1.0);
  radiance *= hit_mask * mat_mask;
  radiance.xyz += uniform_data.constant_ambient.xyz * mat_mask;

  if (material_params.check_finite == 1) {
    bool any_inf = any(isinf(radiance));
    bool any_nan = any(isnan(radiance));
    float finite_val = any_inf || any_nan ? 1.0 : 0.0;
    radiance = vec4(finite_val, finite_val, finite_val, 1.0);
  }
}
