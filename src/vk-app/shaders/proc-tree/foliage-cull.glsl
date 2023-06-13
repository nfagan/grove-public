#version 450

#define MAX_NUM_INSTANCES_PER_CLUSTER (16)

const float INF = 1.0 / 0.0;
const uint MAX_NUM_OCCLUDE_STEPS = 8;
const uint INVALID_LIST = 0xffffffffu;

struct Grid {
  vec3 origin;
  vec3 cell_size;
  ivec3 num_cells;
};

struct GridCellClusterListNode {
  //  <data>
  vec4 inv_frame_x_position_x;
  vec4 inv_frame_y_position_y;
  vec4 inv_frame_z_position_z;
  vec4 half_size;
  uint cluster_group_index;
  uint cluster_offset;
  //  <list>
  uint next;
  uint pad;
};

struct ClusterInstance {
  vec4 position_normal_x;
  vec4 normal_yz_scale_xy;
  uvec4 data0; //  culling (true / false)
};

struct Cluster {
  vec4 aabb_p0;
  vec4 aabb_p1;
  vec4 canonical_position;
  ClusterInstance instances[MAX_NUM_INSTANCES_PER_CLUSTER];
};

struct OcclusionParams {
  float cull_distance_threshold;
};

/*
  resources
*/

layout (std140, set = 0, binding = 0) uniform UniformData {
  vec4 camera_position;
  vec4 grid_origin;
  vec4 grid_cell_size;
  uvec4 grid_num_cells;
};

layout (std430, set = 0, binding = 1) readonly buffer GridCellClusterLists {
  uint grid_cell_cluster_lists[];
};

layout (std430, set = 0, binding = 2) readonly buffer GridCellClusterListNodes {
  GridCellClusterListNode grid_cell_cluster_list_nodes[];
};

layout (std430, set = 0, binding = 3) readonly buffer ClusterGroupOffsets {
  uint cluster_offsets[];
};

layout (std430, ste = 0, binding = 4) buffer Clusters {
  Cluster clusters[];
};

/*
  code
*/

bool is_valid_grid_cell_index(Grid grid, ivec3 index) {
  return all(ge(index, ivec3(0))) && all(lt(index, grid.num_cells));
}

int to_linear_grid_cell_index(Grid grid, ivec3 index) {
  return (grid.num_cells.x * grid.num_cells.y) * index.z + index.y * grid.num_cells.x + index.x;
}

ivec3 to_grid_cell_index(Grid grid, vec3 p) {
  return ivec3(floor((p - grid.origin) / grid.cell_size));
}

float sign_or_zero(float v) {
  return v == 0.0 ? 0.0 : v < 0.0 ? -1.0 : 1.0;
}

vec3 sign_or_zero3(vec3 v) {
  return vec3(sign_or_zero(v.x), sign_or_zero(v.y), sign_or_zero(v.z));
}

vec3 next_cell_bound(Grid grid, vec3 ro_index, vec3 rd) {
  vec3 incr = ro_index + vec3(gt(rd, vec3(0.0)));
  return incr * grid.cell_size + grid.origin;
}

vec3 to_next_cell(vec3 bounds, vec3 ro, vec3 rd) {
  vec3 cs = (bounds - ro) / rd;
  return vec3(
    rd.x == 0.0f ? INF : cs.x,
    rd.y == 0.0f ? INF : cs.y,
    rd.z == 0.0f ? INF : cs.z
  );
}

bool is_culled(ClusterInstance inst) {
  return inst.data0.x != 0;
}

vec3 grid_cell_cluster_list_node_half_size(GridCellClusterListNode node) {
  return node.half_size.xyz;
}

vec3 grid_cell_cluster_list_node_position(GridCellClusterListNode node) {
  return vec3(
    node.inv_frame_x_position_x.w,
    node.inv_frame_y_position_y.w,
    node.inv_frame_z_position_z.w
  );
}

mat3 grid_cell_cluster_list_node_inv_frame(GridCellClusterListNode node) {
  return mat3(
    node.inv_frame_x_position_x.xyz,
    node.inv_frame_y_position_y.xyz,
    node.inv_frame_z_position_z.xyz
  );
}

bool ray_circle_intersect(vec3 ro, vec3 rd, vec3 pp, vec3 pn, float pr) {
  float denom = dot(pn, rd);
  if (denom == 0.0) {
    return false;
  }

  float num = -dot(n, ro) - dot(pn, pp);
  float t = num / denom;
  float r = length((ro + t * rd) - pp);
  return r <= pr;
}

bool ray_aabb_intersect(vec3 ro, vec3 rd, vec3 p0, vec3 p1) {
  float tmp_t0 = -INF;
  float tmp_t1 = INF;

  for (int i = 0; i < 3; i++) {
    float inv_d = 1.0 / rd[i];
    float t00 = (p0[i] - ro[i]) * inv_d;
    float t11 = (p1[i] - ro[i]) * inv_d;

    if (t00 > t11) {
      float tmp = t00;
      t00 = t11;
      t11 = tmp;
    }

    tmp_t0 = max(tmp_t0, t00);
    tmp_t1 = min(tmp_t1, t11);

    if (tmp_t0 > tmp_t1) {
      return false;
    }
  }

  return true;
}

bool ray_node_intersect(vec3 ro, vec3 rd, GridCellClusterListNode node) {
  ro = ro - grid_cell_cluster_list_node_position(node);
  rd = grid_cell_cluster_list_node_inv_frame(node) * rd;
  vec3 hs = grid_cell_cluster_list_node_half_size(node);
  return ray_aabb_intersect(ro, rd, -hs, hs);
}

bool ray_cluster_instance_intersect(vec3 ro, vec3 rd, ClusterInstance inst) {
  vec3 pp = inst.position_normal_x.xyz;
  vec3 pn = vec3(inst.position_normal_x.w, inst.normal_yz_scale_xy.xy);
  float r = max(inst.normal_yz_scale_xy.z, inst.normal_yz_scale_xy.w); //  @TODO
  return ray_circle_intersect(ro, rd, pp, pn, r);
}

bool skip_cluster_list_node(GridCellClusterListNode node, vec3 camera_pos, OcclusionParams params) {
  vec3 p = grid_cell_cluster_list_node_position(node);
  float d = length(p - camera_pos);
  return d >= params.cull_distance_threshold;
}

bool occluded(Grid grid, vec3 camera_pos, vec3 p, OcclusionParams params) {
  vec3 rd = camera_pos - p;
  float len = length(rd);

  if (len == 0.0) {
    return false;
  } else {
    rd /= len;
  }

  vec3 ro = p;
  ro += rd * max(0.0, len - params.cull_distance_threshold);

  ivec3 ro_index = to_grid_cell_index(grid, ro);

  vec3 cs = to_next_cell(next_cell_bound(grid, vec3(ro_index), rd), ro, rd);
  vec3 ts = abs(grid.cell_size / rd);
  ivec3 ss = ivec3(sign_or_zero3(rd));
  ivec3 is = ivec3(0);
  uint step = 0;

  while (step < MAX_NUM_OCCLUDE_STEPS) {
    ivec3 cell_index = ro_index + is;
    if (!is_valid_grid_cell_index(grid, cell_index)) {
      break;
    }

    uint linear_cell_index = to_linear_grid_cell_index(grid, cell_index);
    uint cluster_list = grid_cell_cluster_lists[linear_cell_index];

    while (cluster_list != INVALID_LIST) {
      GridCellClusterListNode node = grid_cell_cluster_list_nodes[cluster_list];

      if (!skip_cluster_list_node(node, camera_pos, params) && ray_node_intersect(ro, rd, node)) {
        uint cluster_index = cluster_group_offsets[node.cluster_group_index] + node.cluster_offset;
        Cluster cluster = clusters[cluster_index];

        for (int i = 0; i < MAX_NUM_INSTANCES_PER_CLUSTER; i++) {
          ClusterInstance cluster_instance = cluster.instances[i];
          if (!is_culled(cluster_instance) &&
              ray_cluster_instance_intersect(ro, rd, cluster_instance)) {
            return true;
          }
        }
      }

      cluster_list = node.next;
    }

    if (cs.x < cs.y && cs.x < cs.z) {
      is.x += ss.x;
      cs.x += ts.x;
    } else if (cs.y < cs.z) {
      is.y += ss.y;
      cs.y += ts.y;
    } else {
      is.z += ss.z;
      cs.z += ts.z;
    }

    step++;
  }

  return false;
}

Grid parse_grid() {
  Grid result;
  result.origin = grid_origin.xyz;
  result.cell_size = grid_cell_size.xyz;
  result.num_cells = ivec3(grid_num_cells.xyz);
  return result;
}

void main() {
  Grid grid = parse_grid();
}
