void unpack_self_child(uvec4 a, out vec4 sf, out vec4 cf) {
  const uvec4 mask = uvec4(0xffff);
  const vec4 maskf = vec4(0xffff);

  uvec4 child = (a >> 16u) & mask;
  uvec4 self = a & mask;

  sf = vec4(self) / maskf * 2.0 - 1.0;
  cf = vec4(child) / maskf * 2.0 - 1.0;
}

mat3 unpack_matrix(vec4 unpack0, vec4 unpack1) {
  vec3 x = vec3(unpack0.x, unpack0.y, unpack0.z);
  vec3 y = vec3(unpack0.w, unpack1.x, unpack1.y);
  vec3 z = cross(y, x);
  return mat3(x, y, z);
}

void unpack_coord_sys(out mat3 self, out mat3 child) {
  vec4 unpack_self0;
  vec4 unpack_child0;
  unpack_self_child(directions0, unpack_self0, unpack_child0);

  vec4 unpack_self1;
  vec4 unpack_child1;
  unpack_self_child(directions1, unpack_self1, unpack_child1);

  self = unpack_matrix(unpack_self0, unpack_self1);
  child = unpack_matrix(unpack_child0, unpack_child1);
}