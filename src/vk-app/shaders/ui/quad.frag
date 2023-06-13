#version 450

layout (location = 0) out vec4 frag_color;

layout (location = 0) in vec3 v_color;
layout (location = 1) in vec3 v_border_color;
layout (location = 2) in vec2 v_centroid_relative_position;
layout (location = 3) in vec2 v_dimensions;
layout (location = 4) in vec2 v_radius_and_border_size;
layout (location = 5) in float v_opacity;

void main() {
  vec3 color = v_color;
  vec3 border_color = v_border_color;

  float w = v_dimensions.x;
  float h = v_dimensions.y;
  float r = v_radius_and_border_size.x;
  float bs = v_radius_and_border_size.y;
  vec2 p = v_centroid_relative_position;

  vec2 cp_tl = vec2(-w * 0.5, h * 0.5) + r * vec2(1.0, -1.0);
  vec2 cp_bl = vec2(-w * 0.5, -h * 0.5) + r * vec2(1.0, 1.0);
  vec2 cp_tr = vec2(-cp_tl.x, cp_tl.y);
  vec2 cp_br = vec2(-cp_bl.x, cp_bl.y);

  float nv = 0.0;
  bool within_radius = false;
  if (p.x < -w * 0.5 + r) {
    if (p.y >= h * 0.5 - r) {
      nv = length(p - cp_tl);
      within_radius = true;
    } else if (p.y < -h * 0.5 + r) {
      nv = length(p - cp_bl);
      within_radius = true;
    }
  } else if (p.x >= w * 0.5 - r) {
    if (p.y >= h * 0.5 - r) {
      nv = length(p - cp_tr);
      within_radius = true;
    } else if (p.y < -h * 0.5 + r) {
      nv = length(p - cp_br);
      within_radius = true;
    }
  }

#if 0
  if (nv > r) {
    discard;
  }
#endif

  float aa_size = 2.0;
  float nv_frac = 1.0 - min(aa_size, (max(nv, r) - r)) / aa_size;
  nv_frac = pow(nv_frac, 2.0);

  bool in_border = within_radius && r - nv < bs;
  if (!in_border) {
    in_border = (w * 0.5 - abs(p.x) < bs) || (h * 0.5 - abs(p.y) < bs);
  }

  if (in_border) {
    float border_dist = max(0.0, abs(p.x) - (w * 0.5 - bs));
    border_dist = max(border_dist, max(0.0, abs(p.y) - (h * 0.5 - bs)));
    border_dist = max(bs - min(bs, max(0.0, r - nv)), border_dist);
    border_dist = min(border_dist, aa_size) / aa_size;
    //    border_dist = sqrt(border_dist);
    color = mix(color, border_color, border_dist);
  }

  frag_color = vec4(color, nv_frac * v_opacity);
}