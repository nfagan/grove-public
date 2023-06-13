#pragma once

#include "growth_on_mesh.hpp"
#include <cstdint>

namespace grove::tree {

struct ProjectedTreeInstanceHandle {
  GROVE_INTEGER_IDENTIFIER_EQUALITY(ProjectedTreeInstanceHandle, id)
  uint32_t id;
};

struct ProjectNodesOntoMeshParams {
  const uint32_t* tris;
  uint32_t num_tris;
  const ProjectRayEdgeIndices* edge_indices;
  const ray_project::NonAdjacentConnections* non_adjacent_connections;
  const Vec3f* ps;
  const Vec3f* ns;
  uint32_t ti;
  double initial_ray_theta_offset;
  double ray_length;
};

struct DefaultProjectNodesOntoMeshResult {
  tree::PostProcessProjectedNodesResult post_process_res;
  std::vector<ProjectRayResultEntry> project_ray_results;
};

struct CreateProjectedTreeInstanceParams {
  float diameter_power;
  float ornament_growth_incr;
  float axis_growth_incr;
};

struct ProjectedNodesSystem {
public:
  struct PendingState {
    bool start_receding;
  };

  enum class GrowthState {
    Idle,
    PreparingToGrow,
    Growing,
    Receding,
  };

  enum class GrowthPhase {
    Idle,
    BranchesGrowing,
    OrnamentsGrowing,
    PreparingToRecede,
    BranchesReceding,
    OrnamentsReceding,
    FinishedReceding
  };

  struct Events {
    bool nodes_created;
    bool branches_modified;
    bool ornaments_modified;
  };

  struct Instance {
    const tree::Internodes* get_internodes() const {
      return &project_result.internodes;
    }
    tree::Internodes* get_internodes() {
      return &project_result.internodes;
    }

    uint32_t id;
    float diameter_power;
    tree::PostProcessProjectedNodesParams post_process_params;
    tree::PostProcessProjectedNodesResult project_result;

    std::unique_ptr<tree::RenderAxisGrowthContext> growth_context;
    std::unique_ptr<tree::RenderAxisDeathContext> death_context;
    std::vector<int> growing_ornament_indices;
    float axis_growth_incr;
    float ornament_growth_frac;
    float ornament_growth_incr{0.01f};
    Optional<TreeNodeIndex> growing_axis_root;
    GrowthState growth_state{};
    GrowthPhase growth_phase{};
    Events events;
    PendingState pending_state{};
  };

  struct UpdateInfo {
    double real_dt;
  };

  struct Stats {
    int num_instances;
    int num_axis_growth_contexts;
    int num_axis_death_contexts;
  };

public:
  std::vector<Instance> instances;
  std::vector<std::unique_ptr<tree::RenderAxisGrowthContext>> render_growth_contexts;
  std::vector<std::unique_ptr<tree::RenderAxisDeathContext>> render_death_contexts;
  uint32_t next_instance_id{1};
};

ProjectedTreeInstanceHandle create_instance(ProjectedNodesSystem* system,
                                            const CreateProjectedTreeInstanceParams& params);
void destroy_instance(ProjectedNodesSystem* system, ProjectedTreeInstanceHandle handle);
const ProjectedNodesSystem::Instance* read_instance(const ProjectedNodesSystem* system,
                                                    ProjectedTreeInstanceHandle handle);

void set_axis_growth_increment(ProjectedNodesSystem* system,
                               ProjectedTreeInstanceHandle handle, float incr);
void set_need_start_receding(ProjectedNodesSystem* system, ProjectedTreeInstanceHandle handle);
bool is_finished_receding(const ProjectedNodesSystem* system, ProjectedTreeInstanceHandle handle);

void project_nodes_onto_mesh(ProjectedNodesSystem* system,
                             ProjectedTreeInstanceHandle instance,
                             const tree::Internodes& src_inodes,
                             const ProjectNodesOntoMeshParams& params);

void emplace_projected_nodes(ProjectedNodesSystem* sys,
                             ProjectedTreeInstanceHandle instance,
                             tree::PostProcessProjectedNodesResult&& proj_res);

DefaultProjectNodesOntoMeshResult
default_project_nodes_onto_mesh(const Internodes& src_inodes,
                                const ProjectNodesOntoMeshParams* proj_params, float diam_power);

ProjectedNodesSystem::Stats get_stats(const ProjectedNodesSystem* system);

void begin_update(ProjectedNodesSystem* system);
void update(ProjectedNodesSystem* system, const ProjectedNodesSystem::UpdateInfo& info);

}