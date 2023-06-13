struct Primitive {
  float p0x, p0y, p0z;  //  tri p0
  float p1x, p1y, p1z;  //  tri p1
  float p2x, p2y, p2z;  //  tri p2
  uint group_id;        //  id of the primitive group (i.e., set of triangles / mesh) to which this prim belongs.
  uint id;              //  unique id of this triangle.
  uint aux_index;       //  index into auxiliary array of additional data, for shading.
  uint pad[4];          //  pad to 64 bytes.
};  //  static_assert(sizeof(Primitive) == 64 && alignof(Primitive) == 4);

struct Instance {
  float m00, m10, m20;        //  transform col 0
  float m01, m11, m21;        //  transform col 1
  float m02, m12, m22;        //  transform col 2
  float m03, m13, m23;        //  transform col 3

  uint id;                    //  unique id of this instance
  uint primitive_node_index;  //  root node of the primitive BVH associated with this instance.
  uint material_info;         //  type and index of material.
  uint pad;                   //  pad to 64 bytes.
};  //  static_assert(sizeof(Instance) == 64 && alignof(Instance) == 4);

struct Node {
  uint left;
  uint right;
  float lb0x, lb0y, lb0z;
  float lb1x, lb1y, lb1z;
  float rb0x, rb0y, rb0z;
  float rb1x, rb1y, rb1z;
  uint self;
  uint parent;
};  //  static_assert(sizeof(Node) == 64 && alignof(Node) == 4);

uint instance_material_index(Instance i) {
  return i.material_info; //  for now.
}

uint instance_material_type(Instance i) {
  return 0; //  for now.
}

mat4 instance_transform(Instance i) {
  vec4 c0 = vec4(i.m00, i.m10, i.m20, 0.0);
  vec4 c1 = vec4(i.m01, i.m11, i.m21, 0.0);
  vec4 c2 = vec4(i.m02, i.m12, i.m22, 0.0);
  vec4 c3 = vec4(i.m03, i.m13, i.m23, 1.0);
  return mat4(c0, c1, c2, c3);
}