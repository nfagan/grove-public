#pragma once

#include "components.hpp"

namespace grove::tree {

int64_t count_num_available_attraction_points(const AttractionPoints& points);
AxisRootInfo compute_axis_root_info(const Internodes& internodes, TreeNodeIndex root_index = 0);

void map_axis(const std::function<void(TreeNodeIndex)>& func,
              Internodes& internodes, TreeNodeIndex root_index = 0);
void map_axis(const std::function<void(TreeNodeIndex)>& func,
              const Internodes& internodes, TreeNodeIndex root_index = 0);

std::vector<Vec3f> collect_leaf_tip_positions(const Internodes& internodes, int max_num = -1);
TreeNodeIndex axis_tip_index(const Internodes& internodes, TreeNodeIndex node);
int max_gravelius_order(const Internodes& internodes);
void reassign_gravelius_order(Internode* internodes, int num_internodes);
std::vector<Vec3f> extract_octree_points(const AttractionPoints& points);
std::vector<TreeNodeIndex> collect_medial_indices(const tree::Internode* internodes,
                                                  int num_internodes,
                                                  TreeNodeIndex axis_root_index);

void validate_internode_relationships(const tree::Internode* internodes, int num_internodes);
void validate_internode_relationships(const tree::Internodes& internodes);

[[nodiscard]] int prune_rejected_axes(const tree::Internode* src, const bool* accepted,
                                      int num_src, tree::Internode* dst, int* dst_to_src);

}