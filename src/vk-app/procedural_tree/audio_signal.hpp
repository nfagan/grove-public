#pragma once

namespace grove::tree {

struct Internode;

void make_wave_from_internodes(
  const tree::Internode* nodes, int num_nodes, float* dst, int num_dst);

}