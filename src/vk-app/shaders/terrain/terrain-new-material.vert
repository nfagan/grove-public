#version 450

layout (location = 0) in vec3 position;

layout (location = 0) out vec3 v_position;
layout (location = 1) out float v_splotch;
layout (location = 2) out vec3 v_light_space_position0;

layout (set = 0, binding = 0) uniform sampler2D height_map_texture;

#define UNIFORM_DATA_SET (0)
#define UNIFORM_DATA_BINDING (1)
#pragma include "terrain/terrain-data.glsl"

layout (set = 0, binding = 2) uniform sampler2D splotch_texture;

float sample_height(vec2 uv) {
  return texture(height_map_texture, uv).r;
}

float sample_splotch(vec2 uv) {
  return texture(splotch_texture, uv).r;
}

void main() {
  vec2 uv = position.xz;
  float height = sample_height(uv);

  vec3 pos = position * 2.0 - 1.0;
  vec4 world_position = model * vec4(pos, 1.0);

  world_position.y = height;

  v_position = world_position.xyz;
  v_splotch = sample_splotch(uv);
  v_light_space_position0 = vec3(sun_light_view_projection0 * world_position);

  gl_Position = projection * view * world_position;
}
