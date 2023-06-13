#pragma once

#include "roots_system.hpp"

namespace grove::cull {
struct FrustumCullData;
}

namespace grove::tree {

struct RenderRootsSystem;
struct RenderBranchNodesData;

struct RenderRootsInstanceHandle {
  GROVE_INTEGER_IDENTIFIER_EQUALITY(RenderRootsInstanceHandle, id)
  uint32_t id;
};

struct CreateRenderRootsInstanceParams {
  RootsInstanceHandle associated_roots;
};

struct RenderRootsSystemUpdateInfo {
  const RootsSystem* roots_system;
  RenderBranchNodesData* branch_nodes_data;
  cull::FrustumCullData* cull_data;
};

RenderRootsSystem* create_render_roots_system();
void update_render_roots_system(RenderRootsSystem* sys, const RenderRootsSystemUpdateInfo& info);
void destroy_render_roots_system(RenderRootsSystem** sys);

RenderRootsInstanceHandle
create_render_roots_instance(RenderRootsSystem* sys, const CreateRenderRootsInstanceParams& params);
void destroy_render_roots_instance(RenderRootsSystem* sys, RenderRootsInstanceHandle handle);

}