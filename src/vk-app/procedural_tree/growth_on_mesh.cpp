#include "growth_on_mesh.hpp"
#include "render.hpp"
#include "bud_fate.hpp"
#include "utility.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/math/constants.hpp"
#include "grove/math/triangle.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace tree;

Internode prepare_new_child(const Internode& parent, int parent_ind) {
  auto res = parent;
  res.parent = parent_ind;
  res.lateral_child = -1;
  res.medial_child = -1;
  return res;
}

template <typename T, typename F>
std::vector<T> smooth_internode_property(const Internodes& src, int adjacent_count,
                                         const F& get_value) {
  if (src.empty()) {
    return {};
  }

  constexpr int max_adjacent_count = 32;
  constexpr int max_count = max_adjacent_count * 2 + 1;
  adjacent_count = std::max(1, std::min(adjacent_count, max_adjacent_count));

  std::vector<T> res_values;
  res_values.resize(src.size());

  std::vector<int> pend_lat;
  pend_lat.push_back(0);
  while (!pend_lat.empty()) {
    int med_ind = pend_lat.back();
    pend_lat.pop_back();
    while (med_ind != -1) {
      auto* med_node = src.data() + med_ind;
      T values[max_count];
      int value_count{};
      //  Traverse parents.
      int prev_count{};
      int parent_ind = med_node->parent;
      while (parent_ind != -1 && prev_count < adjacent_count) {
        values[value_count++] = get_value(parent_ind);
        prev_count++;
        parent_ind = src[parent_ind].parent;
      }
      //  Add the value at the current node.
      values[value_count++] = get_value(med_ind);
      //  Traverse medial children.
      int next_count{};
      int next_ind = med_node->medial_child;
      while (next_ind != -1 && next_count < adjacent_count) {
        values[value_count++] = get_value(next_ind);
        next_count++;
        next_ind = src[next_ind].medial_child;
      }
      assert(value_count <= max_count);
      //  average
      T s{};
      for (int i = 0; i < value_count; i++) {
        s += values[i];
      }
      s /= float(value_count);
      res_values[med_ind] = s;
      //  next medial child
      med_ind = med_node->medial_child;
      if (med_node->has_lateral_child()) {
        pend_lat.push_back(med_node->lateral_child);
      }
    }
  }

  return res_values;
}

void constrain_diameter(tree::Internodes& inodes, float diam) {
  for (auto& node : inodes) {
    node.diameter = std::min(node.diameter, diam);
  }
}

} //  anon

ProjectNodesResult
tree::project_internodes_onto_mesh(const uint32_t* tris, uint32_t num_tris,
                                   const Vec3f* ps, uint32_t ti,
                                   const Vec3<double>& src_p, const tree::Internodes& internodes,
                                   double initial_theta_offset, double length_scale,
                                   const ProjectRayEdgeIndices* edge_indices,
                                   const ray_project::NonAdjacentConnections* non_adjacent_connections) {
  struct NodeStackEntry {
    const tree::Internode* inode;
    ProjectRayNextIteration next;
    double parent_theta;
    int pend_inode;
  };

  assert(tri::is_ccw_or_zero(tris, num_tris, ps));
  const auto* inode_data = internodes.data();
  tree::Internodes result_inodes;
  std::vector<NodeStackEntry> node_stack;

  if (!internodes.empty()) {
    ProjectRayNextIteration first{};
    memcpy(first.tri, tris + ti * 3, 3 * sizeof(uint32_t));
    first.ti = ti;
    first.p = src_p;
    first.ray_theta = initial_theta_offset;

    NodeStackEntry entry{};
    entry.inode = inode_data;
    entry.next = first;
    entry.parent_theta = atan2(inode_data->direction.y, inode_data->direction.x);
    entry.pend_inode = 0;
    node_stack.push_back(entry);

    auto root_inode = prepare_new_child(*inode_data, -1);
    root_inode.position = to_vec3f(first.p);
    root_inode.render_position = root_inode.position;
    result_inodes.push_back(root_inode);
  }

  std::vector<ProjectRayResultEntry> proj_results;
  while (!node_stack.empty()) {
    auto node_info = node_stack.back();
    node_stack.pop_back();
    auto& node = *node_info.inode;
    auto proj_res = project_ray_onto_mesh(
      tris, num_tris, ps, node_info.next, node.length * float(length_scale),
      edge_indices, non_adjacent_connections);

    int pend_inode = node_info.pend_inode;
    int entry_ind{};
    auto num_entries = int(proj_res.entries.size());
    for (auto& entry : proj_res.entries) {
      auto next_p = to_vec3f(entry.exit_p);
      auto next_inode = int(result_inodes.size());
      auto& pend_node = result_inodes[pend_inode];
      auto pend_dir = next_p - pend_node.position;
      auto pend_len = pend_dir.length();
      auto norm_dir = pend_dir / pend_len;
      assert(pend_node.medial_child == -1);
      pend_node.direction = norm_dir;
      pend_node.length = pend_len;
      //  @NOTE, (ab)use of bud indices for referring to ray projection results.
      pend_node.bud_indices[0] = int(proj_results.size());

      if (entry_ind + 1 < num_entries || (node.has_medial_child() && proj_res.completed)) {
        pend_node.medial_child = next_inode;
        auto med_node = prepare_new_child(pend_node, pend_inode);
        med_node.position = next_p;
        med_node.render_position = next_p;
        med_node.direction = norm_dir;
        med_node.length = pend_len;
        result_inodes.push_back(med_node);
        pend_inode = next_inode;
      }

      proj_results.push_back(entry);
      entry_ind++;
    }

    if (proj_res.completed) {
      auto next_theta = atan2(double(node.direction.y), double(node.direction.x));
      auto th_off = next_theta - node_info.parent_theta;
      if (node.has_lateral_child()) {
        auto& curr_inode = result_inodes[node_info.pend_inode];
        assert(curr_inode.lateral_child == -1);
        auto next_inode = curr_inode;
        auto next_inode_ind = int(result_inodes.size());
        next_inode.parent = node_info.pend_inode;
        next_inode.medial_child = -1;
        curr_inode.lateral_child = next_inode_ind;
        result_inodes.push_back(next_inode);

        auto last = node_info.next;
        last.ray_theta += th_off;
        node_stack.push_back({inode_data + node.lateral_child, last, next_theta, next_inode_ind});
      }
      if (node.has_medial_child()) {
        auto next = prepare_next_iteration(proj_res, th_off);
        node_stack.push_back({inode_data + node.medial_child, next, next_theta, pend_inode});
      }
    }
  }

  return {std::move(proj_results), std::move(result_inodes)};
}

void tree::extract_mesh_normals_at_projected_internodes(const Vec3f* ns,
                                                        const ProjectRayResultEntry* ray_proj_results,
                                                        uint32_t num_ray_proj_results,
                                                        const Internodes& internodes,
                                                        Vec3f* dst_ns) {
  (void) num_ray_proj_results;
  int ni{};
  for (auto& node : internodes) {
    const int entry_ind = node.bud_indices[0];
    assert(entry_ind >= 0 && entry_ind < int(num_ray_proj_results));
    auto& entry = ray_proj_results[entry_ind];
    //  @TODO: Consider weighted average of normals from all vertices. Tried this, but saw
    //  worse results when normals vary greatly across vertices.
    dst_ns[ni++] = ns[entry.tri[0]];
  }
}

void tree::offset_internodes_by_normal_and_radius(Internodes& internodes, const Vec3f* ns) {
  for (int i = 0; i < int(internodes.size()); i++) {
    const float radius = internodes[i].diameter * 0.5f;
    auto n_off = ns[i] * radius;
    internodes[i].position += n_off;
    internodes[i].render_position += n_off;
  }
}

void tree::smooth_internode_diameters(Internodes& nodes, int adjacent_count) {
  auto get_diam = [&nodes](int index) {
    return nodes[index].diameter;
  };
  auto tmp_diams = smooth_internode_property<float>(nodes, adjacent_count, get_diam);
  for (size_t i = 0; i < nodes.size(); i++) {
    nodes[i].diameter = tmp_diams[i];
  }
}

void tree::smooth_extracted_mesh_normals(const Internodes& src, Vec3f* ns, int adjacent_count) {
  auto get_normal = [ns](int index) {
    return ns[index];
  };
  auto tmp_normals = smooth_internode_property<Vec3f>(src, adjacent_count, get_normal);
  for (size_t i = 0; i < src.size(); i++) {
    ns[i] = tmp_normals[i];
  }
}

uint32_t tree::find_triangle_containing_min_y_point(const uint32_t* tris, uint32_t num_tris,
                                                    const Vec3f* ps, uint32_t num_ps) {
  uint32_t min_pi_ind{};
  float min_y{infinityf()};
  for (uint32_t i = 0; i < num_ps; i++) {
    if (ps[i].y < min_y) {
      min_y = ps[i].y;
      min_pi_ind = i;
    }
  }

  for (uint32_t i = 0; i < num_tris; i++) {
    for (int j = 0; j < 3; j++) {
      auto ind = tris[i * 3 + j];
      if (ind == min_pi_ind) {
        return i;
      }
    }
  }

  return ~0u;
}

uint32_t tree::find_largest_triangles_containing_lowest_y(const uint32_t* tris, uint32_t num_tris,
                                                          const Vec3f* ps, uint32_t num_ps,
                                                          uint32_t* out, uint32_t max_num_out) {
#if 1
  struct PointInfo {
    Vec3f p;
    uint32_t ti;
    float det;
  };

  (void) num_ps;
  auto tmp = std::make_unique<PointInfo[]>(num_tris * 3);
  for (uint32_t i = 0; i < num_tris; i++) {
    for (int j = 0; j < 3; j++) {
      uint32_t pi = tris[i * 3 + j];
      assert(pi < num_ps);
      tmp[i * 3 + j] = PointInfo{ps[pi], i, 0.0f};
    }
    auto tri_det = det(tmp[i * 3].p, tmp[i * 3 + 1].p, tmp[i * 3 + 2].p);
    for (int j = 0; j < 3; j++) {
      tmp[i * 3 + j].det = tri_det;
    }
  }

  std::sort(tmp.get(), tmp.get() + num_tris * 3, [](const PointInfo& a, const PointInfo& b) {
    return a.det > b.det && a.p.y < b.p.y;
  });

  uint32_t num_out_tris{};
  for (uint32_t i = 0; i < num_tris * 3; i++) {
    if (num_out_tris >= max_num_out) {
      break;
    }
    const uint32_t ti = tmp[i].ti;
    const bool is_new = std::find(out, out + num_out_tris, ti) == (out + num_out_tris);
    if (is_new) {
      out[num_out_tris++] = ti;
    }
  }
  return num_out_tris;
#else
  struct PointInfo {
    float y;
    uint32_t pi;
  };

  uint32_t min_pi{};
  uint32_t max_pi{};
  if (num_tris > 0) {
    min_pi = *std::min_element(tris, tris + num_tris * 3);
    max_pi = *std::max_element(tris, tris + num_tris * 3);
  }

  Temporary<PointInfo, 64> tmp_info_store;
  PointInfo* tmp_info = tmp_info_store.require(int(max_num_out));
  uint32_t num_tmp_info{};

  for (uint32_t i = 0; i < num_ps; i++) {
    if (i < min_pi || i > max_pi) {
      continue;
    }
    float query_y = ps[i].y;
    uint32_t insert_at{};
    while (insert_at < num_tmp_info) {
      if (query_y < tmp_info[insert_at].y) {
        break;
      }
      insert_at++;
    }
    if (insert_at < max_num_out) {
      num_tmp_info = std::min(num_tmp_info + 1, max_num_out);
      for (uint32_t j = num_tmp_info; j > insert_at + 1; j--) {
        assert(j >= 2);
        tmp_info[j-1] = tmp_info[j-2];
      }
      tmp_info[insert_at] = {query_y, i};
    }
  }

  uint32_t num_out_tris{};
  for (uint32_t i = 0; i < num_tris * 3 && num_out_tris < max_num_out; i++) {
    uint32_t pi = tris[i];
    const uint32_t ti = i / 3;
    for (uint32_t j = 0; j < num_tmp_info; j++) {
      if (tmp_info[j].pi != pi) {
        continue;
      }
      bool has_tri{};
      for (uint32_t k = 0; k < num_out_tris; k++) {
        if (out[k] == ti) {
          has_tri = true;
          break;
        }
      }
      if (!has_tri) {
        out[num_out_tris++] = ti;
      }
    }
  }

  return num_out_tris;
#endif
}

//  We want to remove branches that cross over one another. But we can't accept or reject an
//  individual internode depending on whether it intersects any other internode, because there are
//  several instances where we actually do expect internodes to overlap one another, and where it
//  looks plausible.
//
//  The intuition is that we allow child branches to be initially embedded within their parent
//  branch, but once a child branch "emerges" (i.e., becomes un-embedded) from its parent, we prune
//  it if/where it intersects with another branch.
//
//  Here is the strategy:
//    Accept all medial nodes on the root axis (branch) and push all child axes to a pending stack.
//    Pop a child axis from the stack. While a node on the child axis intersects the parent axis,
//      accept the intersecting node, but skip its lateral child if it has one.
//    For each remaining node on the axis, consider whether it intersects with any other node
//      that has already been accepted. If it does intersect another node, check whether the
//      intersecting node is one of the N medial parents of the current node. If it isn't, then
//      reject the current node. Otherwise, if there are no intersections or all intersected nodes
//      are among the N medial parents of the current node, accept the current node and push its
//      lateral child to the pending stack if it has one. This part is basically a hack because we
//      use OBBs to represent the internodes, so a child node is almost guaranteed to intersect
//      its medial parent if its direction changes at all.
Internodes tree::prune_intersecting(const Internodes& inodes, int queue_size, float obb_diam) {
  struct PendNode {
    int src_self_ind;
    int dst_parent_ind;
    int src_parent_axis_root_ind;
  };

  struct ResultNodeMeta {
    OBB3f obb;
    int src_axis_root_ind;
    int src_self_ind;
  };

  tree::Internodes result;
  if (inodes.empty()) {
    return result;
  }

  const auto make_node_obb = [obb_diam](const Internode& node) {
    if (obb_diam <= 0.0f) {
      return tree::internode_obb(node);
    } else {
      return tree::internode_obb_custom_diameter(node, obb_diam);
    }
  };

  const auto make_result_node_meta = [](const OBB3f& obb, int axis_root, int self) {
    ResultNodeMeta result;
    result.obb = obb;
    result.src_axis_root_ind = axis_root;
    result.src_self_ind = self;
    return result;
  };

  const bool use_queue = queue_size > 0;
  const auto* src_nodes = inodes.data();
  std::vector<PendNode> pend_stack;
  std::vector<ResultNodeMeta> result_meta;
  {
    //  Push the root axis.
    int src_self_ind = 0;
    int dst_parent_ind = -1;
    while (src_self_ind != -1) {
      auto *src_node = src_nodes + src_self_ind;
      int dst_self_ind = int(result.size());
      if (dst_parent_ind >= 0) {
        assert(result[dst_parent_ind].medial_child == -1);
        result[dst_parent_ind].medial_child = dst_self_ind;
      }

      auto dst_node = prepare_new_child(*src_node, dst_parent_ind);
      result.push_back(dst_node);

      auto meta = make_result_node_meta(make_node_obb(dst_node), 0, src_self_ind);
      result_meta.push_back(meta);

      if (src_node->has_lateral_child()) {
        PendNode pend{};
        pend.src_self_ind = src_node->lateral_child;
        pend.dst_parent_ind = dst_self_ind;
        pend.src_parent_axis_root_ind = 0;
        pend_stack.push_back(pend);
      }
      src_self_ind = src_node->medial_child;
      dst_parent_ind = dst_self_ind;
    }
  }

  int src_queue_size = queue_size;
  const auto push_queue = [src_queue_size](std::vector<int>& q, int s) {
    if (int(q.size()) < src_queue_size) {
      q.push_back(s);
    } else {
      std::rotate(q.begin(), q.begin() + 1, q.end());
      q.back() = s;
    }
  };

  std::vector<int> src_queue;
  while (!pend_stack.empty()) {
    auto pend = pend_stack.back();
    pend_stack.pop_back();
    if (use_queue) {
      src_queue.clear();
    }

    int dst_parent_ind = pend.dst_parent_ind;
    int src_self_ind = pend.src_self_ind;
    const int src_axis_root_ind = src_self_ind;
    bool expect_lateral_child = true;
    while (src_self_ind != -1) {
      auto* src_node = src_nodes + src_self_ind;
      bool hit_parent = false;
      auto src_obb = make_node_obb(*src_node);
      for (auto& meta : result_meta) {
        if (obb_obb_intersect(src_obb, meta.obb) &&
            meta.src_axis_root_ind == pend.src_parent_axis_root_ind) {
          hit_parent = true;
          break;
        }
      }
      if (!hit_parent) {
        //  Stop once the node has become un-embedded from its parent.
        break;
      }
      if (use_queue) {
        push_queue(src_queue, src_self_ind);
      }

      auto dst_self_ind = int(result.size());
      auto dst_node = prepare_new_child(*src_node, dst_parent_ind);
      result.push_back(dst_node);

      auto meta = make_result_node_meta(make_node_obb(dst_node), src_axis_root_ind, src_self_ind);
      result_meta.push_back(meta);

      if (dst_parent_ind >= 0) {
        if (expect_lateral_child) {
          assert(result[dst_parent_ind].lateral_child== -1);
          result[dst_parent_ind].lateral_child = dst_self_ind;
          expect_lateral_child = false;
        } else {
          assert(result[dst_parent_ind].medial_child == -1);
          result[dst_parent_ind].medial_child = dst_self_ind;
        }
      }
      src_self_ind = src_node->medial_child;
      dst_parent_ind = dst_self_ind;
    }

    while (src_self_ind != -1) {
      auto* src_node = src_nodes + src_self_ind;
      auto src_obb = make_node_obb(*src_node);
      bool hit_other = false;
      for (auto& meta : result_meta) {
        if (!use_queue) {
          if (obb_obb_intersect(src_obb, meta.obb) && meta.src_self_ind != src_node->parent) {
            hit_other = true;
            break;
          }
        } else {
          if (obb_obb_intersect(src_obb, meta.obb)) {
            bool one_of_parents = false;
            for (auto s : src_queue) {
              if (s == meta.src_self_ind) {
                one_of_parents = true;
                break;
              }
            }
            if (!one_of_parents) {
              hit_other = true;
              break;
            }
          }
        }
      }
      if (hit_other) {
        break;
      }
      if (use_queue) {
        push_queue(src_queue, src_self_ind);
      }
      auto dst_self_ind = int(result.size());
      auto dst_node = prepare_new_child(*src_node, dst_parent_ind);
      result.push_back(dst_node);

      auto meta = make_result_node_meta(make_node_obb(*src_node), src_axis_root_ind, src_self_ind);
      result_meta.push_back(meta);

      if (src_node->has_lateral_child()) {
        PendNode next_pend{};
        next_pend.src_self_ind = src_node->lateral_child;
        next_pend.dst_parent_ind = dst_self_ind;
        next_pend.src_parent_axis_root_ind = src_axis_root_ind;
        pend_stack.push_back(next_pend);
      }
      if (dst_parent_ind >= 0) {
        if (expect_lateral_child) {
          assert(result[dst_parent_ind].lateral_child == -1);
          result[dst_parent_ind].lateral_child = dst_self_ind;
          expect_lateral_child = false;
        } else {
          assert(result[dst_parent_ind].medial_child == -1);
          result[dst_parent_ind].medial_child = dst_self_ind;
        }
      }
      src_self_ind = src_node->medial_child;
      dst_parent_ind = dst_self_ind;
    }
  }

  return result;
}

PostProcessProjectedNodesResult
tree::post_process_projected_internodes(Internodes inodes,  //  @NOTE: by value
                                        const tree::SpawnInternodeParams& spawn_params,
                                        const Vec3f* mesh_normals,
                                        const ProjectRayResultEntry* proj_ray_results,
                                        uint32_t num_proj_ray_results,
                                        const PostProcessProjectedNodesParams& params) {
  if (params.prune_intersecting_internode_queue_size > 0) {
    inodes = prune_intersecting(inodes, params.prune_intersecting_internode_queue_size, 0.0f);
  }
  if (params.reset_internode_diameter) {
    tree::set_diameter(inodes, spawn_params);
  }
  if (params.smooth_diameter_adjacent_count > 0) {
    tree::smooth_internode_diameters(inodes, params.smooth_diameter_adjacent_count);
  }
  if (params.max_diameter) {
    assert(params.max_diameter.value() > 0.0f);
    constrain_diameter(inodes, params.max_diameter.value());
  }

  std::vector<Vec3f> true_normals(inodes.size());
  extract_mesh_normals_at_projected_internodes(
    mesh_normals, proj_ray_results, num_proj_ray_results, inodes, true_normals.data());

  auto processed_mesh_normals = true_normals;
  if (params.smooth_normals_adjacent_count > 0) {
    const int adj_count = params.smooth_normals_adjacent_count;
    tree::smooth_extracted_mesh_normals(inodes, processed_mesh_normals.data(), adj_count);
  }

  if (params.offset_internodes_by_radius) {
    tree::offset_internodes_by_normal_and_radius(inodes, processed_mesh_normals.data());
  }

  if (params.constrain_lateral_child_diameter) {
    constrain_lateral_child_diameter(inodes);
  }

  tree::reassign_gravelius_order(inodes.data(), int(inodes.size()));
  if (!params.preserve_source_internode_ids) {
    for (auto& node : inodes) {
      node.id = TreeInternodeID::create();
    }
  }

  PostProcessProjectedNodesResult result;
  result.internodes = std::move(inodes);
  result.processed_mesh_normals = std::move(processed_mesh_normals);
  result.true_mesh_normals = std::move(true_normals);
  return result;
}

GROVE_NAMESPACE_END
