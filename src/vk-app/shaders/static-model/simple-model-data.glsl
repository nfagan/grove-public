layout (set = UNIFORM_SET, binding = UNIFORM_BINDING, std140) uniform ModelData {
  mat4 projection;
  mat4 view;
  mat4 model;
  mat4 sun_light_view_projection0;
  vec4 camera_position;
  vec4 sun_color;
  vec4 sun_position;
};

struct VS_OUT {
  vec3 position;
  vec3 light_space_position0;
  vec3 normal;
  vec2 uv;
};