vec2 unpack_axis_root_info_component(uint component) {
  const uint mask = 0xffff;
  uint a = (component & ~mask) >> 16;
  uint b = component & mask;
  float af = float(a) / float(mask);
  float bf = float(b) / float(mask);
  return vec2(af, bf);
}

void unpack_axis_root_info(in uvec4 root_info, out vec4 parent_info, out vec4 child_info) {
  vec2 xs = unpack_axis_root_info_component(root_info.x);
  vec2 ys = unpack_axis_root_info_component(root_info.y);
  vec2 zs = unpack_axis_root_info_component(root_info.z);
  vec2 actives = unpack_axis_root_info_component(root_info.w);

  parent_info.x = xs.x;
  child_info.x = xs.y;

  parent_info.y = ys.x;
  child_info.y = ys.y;

  parent_info.z = zs.x;
  child_info.z = zs.y;

  parent_info.w = actives.x;
  child_info.w = actives.y;
}
