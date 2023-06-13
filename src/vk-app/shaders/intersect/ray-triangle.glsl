int detail__max_dimension(vec3 a) {
  float m = a.x;
  int i = 0;

  if (a.y > m) {
    m = a.y;
    i = 1;
  }

  if (a.z > m) {
    i = 2;
  }

  return i;
}

bool ray_triangle_intersect(vec3 ro, vec3 rd, vec3 p0, vec3 p1, vec3 p2, out float t) {
  t = 0.0;

  vec3 p0t = p0 - ro;
  vec3 p1t = p1 - ro;
  vec3 p2t = p2 - ro;

  //  Permute dimensions so that largest component of `rd` forms the z-axis.
  int zi = detail__max_dimension(abs(rd));
  int xi = (zi + 1) % 3;
  int yi = (xi + 1) % 3;

  vec3 rdt = vec3(rd[xi], rd[yi], rd[zi]);
  p0t = vec3(p0t[xi], p0t[yi], p0t[zi]);
  p1t = vec3(p1t[xi], p1t[yi], p1t[zi]);
  p2t = vec3(p2t[xi], p2t[yi], p2t[zi]);

  //  Shear components according to transformation that aligns `rd`
  //  with the positive z-axis.
  vec3 shear = vec3(-rdt.x/rdt.z, -rdt.y/rdt.z, 1.0/rdt.z);

  p0t.x += shear.x * p0t.z;
  p0t.y += shear.y * p0t.z;
  p0t.z *= shear.z;

  p1t.x += shear.x * p1t.z;
  p1t.y += shear.y * p1t.z;
  p1t.z *= shear.z;

  p2t.x += shear.x * p2t.z;
  p2t.y += shear.y * p2t.z;
  p2t.z *= shear.z;

  float e0 = p1t.x * p2t.y - p1t.y * p2t.x;
  float e1 = p2t.x * p0t.y - p2t.y * p0t.x;
  float e2 = p0t.x * p1t.y - p0t.y * p1t.x;

#if REEVALUATE_RAY_TRI_ISECT_IN_DOUBLE
  if (e0 == 0 || e1 == 0 || e2 == 0) {
    // Evaluate in double.
    double p2txp1ty = double(p2t.x) * double(p1t.y);
    double p2typ1tx = double(p2t.y) * double(p1t.x);
    e0 = float(p2typ1tx - p2txp1ty);

    double p0txp2ty = double(p0t.x) * double(p2t.y);
    double p0typ2tx = double(p0t.y) * double(p2t.x);
    e1 = float(p0typ2tx - p0txp2ty);

    double p1txp0ty = double(p1t.x) * double(p0t.y);
    double p1typ0tx = double(p1t.y) * double(p0t.x);
    e2 = float(p1typ0tx - p1txp0ty);
  }
#endif

  if ((e0 < 0 || e1 < 0 || e2 < 0) &&
      (e0 > 0 || e1 > 0 || e2 > 0)) {
      //  sign mismatch
    return false;
  }

  float det = e0 + e1 + e2;
  if (det == 0) {
    return false;
  }

  float t_scaled = e0 * p0t.z + e1 * p1t.z + e2 * p2t.z;
  if ((det < 0 && t_scaled >= 0) || (det > 0 && t_scaled <= 0)) {
    return false;
  }

  t = t_scaled / det;
  return true;
}
