bool ray_triangle_intersect(vec3 ro, vec3 rd, vec3 v0, vec3 v1, vec3 v2, out float t) {
  // compute plane's normal
  vec3 v0v1 = v1 - v0;
  vec3 v0v2 = v2 - v0;
  // no need to normalize
  vec3 N = cross(v0v1, v0v2); // N

  // check if ray and plane are parallel ?
  float NdotRayDirection = dot(N, rd);
  if (abs(NdotRayDirection) < 0.001) {
    return false; // they are parallel so they don't intersect !
  }

  // compute d parameter using equation 2
  float d = dot(N, v0);

  // compute t (equation 3)
  t = (dot(N, ro) + d) / NdotRayDirection;
  // check if the triangle is in behind the ray
  if (t < 0.0) {
    return false; // the triangle is behind
  }

  // compute the intersection point using equation 1
  vec3 P = ro + t * rd;

  // Step 2: inside-outside test
  vec3 C; // vector perpendicular to triangle's plane

  // edge 0
  vec3 vp0 = P - v0;
  C = cross(v0v1, vp0);
  if (dot(N, C) > 0.0) {
    return false; // P is on the right side
  }

  // edge 1
  vec3 edge1 = v2 - v1;
  vec3 vp1 = P - v1;
  C = cross(edge1, vp1);
  if (dot(N, C) < 0.0) {
    return false; // P is on the right side
  }

  // edge 2
  vec3 edge2 = v0 - v2;
  vec3 vp2 = P - v2;
  C = cross(edge2, vp2);
  if (dot(N, C) < 0.0) {
    return false; // P is on the right side;
  }

  return true; // this ray hits the triangle
} 