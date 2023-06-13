#version 450 core

#define DEBUG_OUTPUT (0)

#if DEBUG_OUTPUT
layout (location = 0) out vec4 debug_output;
#else
layout (location = 0) out uvec4 ray_intersect_info;
#endif

#define LEAF_MASK (0x80000000u)
#define DATA_SENTINEL (0xffffffffu)
#define NO_HIT (0xffffffffu)
#define INF_LIKE (100000000.0)
#define INSTANCE_STACK_SIZE (16)
#define PRIMITIVE_STACK_SIZE (16)
#define REEVALUATE_RAY_TRI_ISECT_IN_DOUBLE (0)

#pragma include "gi/bvh-data.glsl"

struct TraverseInfo {
  float min_t;
  uint hit_primitive_index;
  uint hit_instance_index;
  vec2 hit_uv;
  uint num_visited_nodes;
};

layout (std430, set = 0, binding = 0) readonly buffer Primitives {
  Primitive primitives[];
};

layout (std430, set = 0, binding = 1) readonly buffer Instances {
  Instance instances[];
};

layout (std430, set = 0, binding = 2) readonly buffer InstanceBVH {
  Node instance_nodes[];
};

layout (std430, set = 0, binding = 3) readonly buffer PrimitiveBVH {
  Node primitive_nodes[];
};

layout (set = 0, binding = 4) uniform sampler2D ray_direction_texture;
layout (set = 0, binding = 5) uniform sampler2D ray_origin_texture;

#pragma include "gi/index.glsl"
#pragma include "intersect/ray-triangle.glsl"
#pragma include "intersect/ray-aabb-test.glsl"

bool is_sentinel(Instance instance) {
  return instance.id == DATA_SENTINEL;
}

bool is_sentinel(Primitive prim) {
  return prim.id == DATA_SENTINEL;
}

bool is_leaf(uint i) {
  return (i & LEAF_MASK) > 0;
}

uint decode_data_index(uint i) {
  return i & (~LEAF_MASK);
}

void gather_vertices(Primitive prim, out vec3 p0, out vec3 p1, out vec3 p2) {
  p0 = vec3(prim.p0x, prim.p0y, prim.p0z);
  p1 = vec3(prim.p1x, prim.p1y, prim.p1z);
  p2 = vec3(prim.p2x, prim.p2y, prim.p2z);
}

bool ray_aabb_intersect(vec3 ro, vec3 rd,
                        float p0x, float p0y, float p0z,
                        float p1x, float p1y, float p1z) {
  vec3 mn = vec3(p0x, p0y, p0z);
  vec3 mx = vec3(p1x, p1y, p1z);
  return ray_aabb_intersect_test(ro, rd, mn, mx);
}

vec2 to_barycentric(vec3 p0, vec3 p1, vec3 p2, vec3 p_hit) {
  mat2x3 m = mat2x3(p0 - p2, p1 - p2);
  mat3x2 mt = transpose(m);
  return inverse(mt * m) * mt * (p_hit - p2);
}

void test_primitives(uint begin, uint instance_index, vec3 ro, vec3 rd, inout TraverseInfo info) {
  while (true) {
    Primitive data = primitives[begin++];
    if (is_sentinel(data)) {
      break;
    } else {
      float t = 0.0;
      vec3 p0, p1, p2;
      gather_vertices(data, p0, p1, p2);
      if (ray_triangle_intersect(ro, rd, p0, p1, p2, t) && t < info.min_t) {
        info.min_t = t;
        info.hit_primitive_index = begin - 1;
        info.hit_instance_index = instance_index;
        info.hit_uv = to_barycentric(p0, p1, p2, ro + rd * t);
      }
    }
  }
}

void traverse_primitive_bvh(uint root, uint instance_index, vec3 ro, vec3 rd, inout TraverseInfo info) {
  uint stack[PRIMITIVE_STACK_SIZE];
  stack[0] = root;
  uint stack_size = 1;

  while (stack_size > 0) {
    Node node = primitive_nodes[stack[--stack_size]];

    if (is_leaf(node.left)) {
      uint begin = decode_data_index(node.left);
      test_primitives(begin, instance_index, ro, rd, info);

    } else if (stack_size < PRIMITIVE_STACK_SIZE &&
               ray_aabb_intersect(ro, rd, node.lb0x, node.lb0y, node.lb0z,
                                          node.lb1x, node.lb1y, node.lb1z)) {
      stack[stack_size++] = node.left;
    }

    if (is_leaf(node.right)) {
      uint begin = decode_data_index(node.right);
      test_primitives(begin, instance_index, ro, rd, info);

    } else if (stack_size < PRIMITIVE_STACK_SIZE &&
               ray_aabb_intersect(ro, rd, node.rb0x, node.rb0y, node.rb0z,
                                          node.rb1x, node.rb1y, node.rb1z)) {
      stack[stack_size++] = node.right;
    }
  }
}

void test_instances(uint begin, vec3 ro, vec3 rd, inout TraverseInfo info) {
  while (true) {
    Instance instance = instances[begin++];
    if (is_sentinel(instance)) {
      break;
    } else {
      uint root = instance.primitive_node_index;
      mat4 inv_trans = inverse(instance_transform(instance));
      vec4 ro_obj = inv_trans * vec4(ro, 1.0);
      vec4 rd_obj = inv_trans * vec4(rd, 0.0);
      traverse_primitive_bvh(root, begin - 1, ro_obj.xyz, rd_obj.xyz, info);
    }
  }
}

uvec4 encode_output(TraverseInfo info) {
  uvec2 uv = floatBitsToUint(clamp(info.hit_uv, vec2(0.0), vec2(1.0)));
  return uvec4(info.hit_primitive_index, info.hit_instance_index, uv.x, uv.y);
}

void main() {
  //  Direction of current ray. Direct look-up because the framebuffer is the same size as `ray_direction_texture`.
  vec3 ray_direction = texelFetch(ray_direction_texture, ivec2(gl_FragCoord.xy), 0).rgb;

  //  Get origin of current ray, i.e. the position of the current probe.
  vec3 ro = texelFetch(ray_origin_texture, ivec2(gl_FragCoord.xy), 0).rgb;
  vec3 rd = ray_direction;

  //  Trace
  uint instance_stack[INSTANCE_STACK_SIZE];
  instance_stack[0] = 0;
  uint instance_stack_size = 1;

  TraverseInfo info;
  info.min_t = INF_LIKE;
  info.hit_primitive_index = NO_HIT;
  info.hit_instance_index = NO_HIT;
  info.hit_uv = vec2(0.0);
  info.num_visited_nodes = 0;

  if (dot(rd, rd) < 0.001) {
    ray_intersect_info = encode_output(info);
    return;
  }

  while (instance_stack_size > 0) {
    Node node = instance_nodes[instance_stack[--instance_stack_size]];
    info.num_visited_nodes++;

    if (is_leaf(node.left)) {
      uint begin = decode_data_index(node.left);
      test_instances(begin, ro, rd, info);

    } else if (instance_stack_size < INSTANCE_STACK_SIZE &&
               ray_aabb_intersect(ro, rd, node.lb0x, node.lb0y, node.lb0z,
                                          node.lb1x, node.lb1y, node.lb1z)) {
      instance_stack[instance_stack_size++] = node.left;
    }

    if (is_leaf(node.right)) {
      uint begin = decode_data_index(node.right);
      test_instances(begin, ro, rd, info);

    } else if (instance_stack_size < INSTANCE_STACK_SIZE &&
               ray_aabb_intersect(ro, rd, node.rb0x, node.rb0y, node.rb0z,
                                          node.rb1x, node.rb1y, node.rb1z)) {
      instance_stack[instance_stack_size++] = node.right;
    }
  }

#if DEBUG_OUTPUT
  float v = info.hit_primitive_index == NO_HIT ? 0.0 : 1.0;
  float ct = float(min(32, info.num_visited_nodes)) / 32.0;
  debug_output = vec4(ct, v, v, 1.0);
#else
  ray_intersect_info = encode_output(info);
#endif
}